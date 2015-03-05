#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

#include "skiplist.h"
#include "garbage.h"
#include "urcu.h"
#include "background.h"

// For testing purposes. To be removed later.
int max_allowed_gap = MAX_GAP * 10;

// Functions used only within this file.
int max_idx_gap(idx_t *idx);
void *bg_thread_fn(void *targs);

// Background thread stuff
pthread_t bg_thread;
_Atomic(int) bg_shouldstop;

// Starts the background thread. num_threads is the number of threads performing
// operations, the background thread's id will be num_threads.
void bg_start(intset_t *set, int num_threads) {
	struct bg_arg *arg = malloc(sizeof(struct bg_arg));
	arg->set = set;
	arg->num_threads = num_threads;
	atomic_store(&bg_shouldstop, 0);
	if (pthread_create(&bg_thread, NULL, bg_thread_fn, arg) != 0) {
		perror("pthread_create");
		exit(1);
	}
}

// Blocks until the background thread finishes.
void bg_stop(void) {
	atomic_store(&bg_shouldstop, 1);
	pthread_join(bg_thread, NULL);
}

// Pthread entry point for the background thread.
void *bg_thread_fn(void *targs) {
	struct bg_arg *arg = (struct bg_arg *) targs;
	intset_t *set = arg->set;
	int num_threads = arg->num_threads;

	urcu_register(num_threads);
	gc_register(num_threads);

	idx_t *spareidx = new_idx(10);
	int ntimes = 0;

	while (atomic_load(&bg_shouldstop) == 0) {
		usleep(250);
		ntimes++;

		// Seize the freelist, we'll free things after the next rcu_sync.
		node_t *freelist = gc_cut();

		// TODO: Doing this check conditionally seems to speed things up on AMD
		// and slow them down on intel. Investigate.
		int maxgap = max_idx_gap();
		if (maxgap > max_allowed_gap) {
			spareidx = restructure(set, spareidx);
		}

		// Wait for an RCU grace period: after this we know that no-one has references
		// to nodes on the portion of the freelist we're holding, and also no-one is
		// looking at spareidx.
		urcu_synchronize();
		gc_free_list(freelist);

		// Run a search for the end help remove the physically deleted nodes.
		set_scanall(set);
	}

	printf("Background thread looped %d times\n", ntimes);

	free_idx(spareidx);
	free(targs);
	urcu_unregister();

	return NULL;
}

// max_idx_gap follows each item in the index, and finds the "gap" between it and
// the next item in the index. It also considers the space from the last index element
// to the end of the list a gap. For example, if the list were [-1*, 1, 2, 5*, 6*, 9] where
// a * represents an indexed node, the maxgap would be 2.
int max_idx_gap(idx_t *idx) {
	int maxgap = 0;
	for (int i = 0; i < idx->size; i++) {
		node_t *curr = idx->elems[i].node;
		node_t *stop = (i == idx->size-1) ? NULL : idx->elems[i+1].node;
		int gap = 0;
		while (curr != stop) {
			gap++;
			curr = curr->next;
		}
		if (gap > maxgap) maxgap = gap;
	}
	return maxgap;
}

// Restructure the index, using the given spareidx. Returns the new spare (the
// index that used to be part of the set).
idx_t *restructure(intset_t *set, idx_t *spareidx) {
	// Start creating the next index. It needs to contain the list head:
	spareidx->elems[0].k = set->head->k;
	spareidx->elems[0].node = set->head;

	// Arrpos: next free slot in elems array.
	int arrpos = 1;

	// Listpos: position in linked list, only counting non-deleted elements.
	int listpos = 0;

	// We need to start past the head node, otherwise we'll try to physically remove it!
	for (node_t *curr = set->head->next; curr != NULL; curr = curr->next) {
		void *val = curr->v;

		// Is the node logically removed? Try to mark it for physical removal.
		if (val == NULL) {
			try_mark_phys_remove(curr);
			continue;
		}

		// Is the node physically removed? Ignore it.
		if (val == curr)
			continue;

		// Now we have a present node.
		listpos++;
		if (listpos % IDX_GAP == 0) {
			// Realloc space in array if needed
			if (arrpos == spareidx->cap) {
				spareidx->cap *= 2;
				spareidx->elems = realloc(spareidx->elems, spareidx->cap * sizeof(idx_elem_t));
			}

			spareidx->elems[arrpos++] = (idx_elem_t) {
				curr->k, curr
			};
		}
	}
	spareidx->size = arrpos;

	// Swap the current index with the spare
	idx_t *current = atomic_load(&set->idx);
	atomic_store(&set->idx, spareidx);
	
	return current;
}