#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

#include "urcu.h"
#include "skiplist.h"
#include "garbage.h"
#include "background.h"

// Operation types passed into the do_operation function.
typedef enum {
	OP_CONTAINS,
	OP_INSERT,
	OP_REMOVE,
} op_t;

// Results returned from the finish_* functions.
typedef enum {
	RESULT_RETRY,
	RESULT_FALSE,
	RESULT_TRUE,
} result_t;

// Functions used only within this file.
int do_operation(intset_t *set, op_t op, key_t k, void *v, int fast);
result_t finish_contains(key_t k, node_t *node, void *node_val);
result_t finish_insert(key_t k, void *v, node_t *node, void *node_val, node_t *next);
result_t finish_remove(key_t k, node_t *node, void *node_val);
node_t *new_node(node_t *prev, node_t *next, key_t k, void *v);
node_t *use_idx(intset_t *set, key_t k);

// Interface functions to use the dictionary as a set.
int set_contains(intset_t *set, key_t key) {
	return do_operation(set, OP_CONTAINS, key, NULL, 1);
}
int set_insert(intset_t *set, key_t key) {
	return do_operation(set, OP_INSERT, key, (void *)(uintptr_t)key, 1);
}
int set_remove(intset_t *set, key_t key) {
	return do_operation(set, OP_REMOVE, key, NULL, 1);
}
void set_scanall(intset_t *set) {
	do_operation(set, OP_CONTAINS, KEY_MAX, NULL, 0);
}

// use_idx returns the element in the index with the largest key less than or
// equal to the given key.
node_t *use_idx(intset_t *set, key_t k) {
	// The index can be swapped out at any point by the background thread.
	idx_t *idx = atomic_load(&set->idx);

	// Binary search using inclusive bounds, answer should be lo.
	int lo = 0, hi = idx->size-1, mid;
	while (hi - lo > 1) {
		mid = (hi + lo) / 2;
		if (idx->elems[mid].k <= k) lo = mid;
		else                        hi = mid-1;
	}
	return idx->elems[lo].node;
}

// do_operation consists of two steps: the search and the operation.
// The search will use the index (if fast == 1), backtracking node.prev links,
// and the help_remove function to find a pair of nodes (node, next) satisfying:
// 1. node.next == next (node next may be NULL)
// 2. node.k <= k < next.k (if next==NULL, then only the first inequality holds).
// 3. Both node and next are not physically deleted.
//
// After this search phase, the relevant finish_* operation is dispatched. If the
// finisher returns RESULT_TRUE or RESULT_FALSE, the operation was successful.
// If it returns RESULT_RETRY, then we restart from the backtracking phase of the
// search step.
int do_operation(intset_t *set, op_t op, key_t k, void *v, int fast) {
	node_t *node, *next;
	void *node_val;
	result_t result;

	urcu_read_lock();
	if (fast) node = use_idx(set, k);
	else      node = set->head;
	for (;;) {
		// Backtrack until we find still present nodes
		while (node == (node_val = node->v))
			node = node->prev;

		// Load the next node
		next = node->next;

		// Check that next is not physically removed.
		if (NULL != next && next == next->v) {
			help_remove(node, next);
			continue;
		}

		// Check finally whether we have node.k <= k < next.k: since all
		// keys in the list are distinct, we only need check the second inequality.
		if (NULL == next || next->k > k) {
			if (op == OP_CONTAINS)
				result = finish_contains(k, node, node_val);
			else if (op == OP_INSERT)
				result = finish_insert(k, v, node, node_val, next);
			else // op == OP_REMOVE
				result = finish_remove(k, node, node_val);

			if (result == RESULT_RETRY)
				continue;
			else
				break;
		}
		node = next;
	}
	urcu_read_unlock();

	return result == RESULT_TRUE;
}

// Each of these finishers perform one short task, and then return one of RESULT_TRUE,
// RESULT_FALSE, or RESULT_RETRY. Retrying is handled by the do_operation function.
result_t finish_contains(key_t k, node_t *node, void *node_val) {
	if (node->k == k && node_val != NULL)
		return RESULT_TRUE;
	return RESULT_FALSE;
}
result_t finish_insert(key_t k, void *v, node_t *node, void *node_val, node_t *next) {
	if (node->k == k) {
		// If not logically deleted, cannot insert.
		if (node_val != NULL)
			return RESULT_FALSE;

		// Attempt to logically insert
		if (atomic_compare_exchange_strong(&node->v, &node_val, v))
			return RESULT_TRUE;

		return RESULT_RETRY;
	}

	node_t *newnode = new_node(node, next, k, v);
	if (atomic_compare_exchange_strong(&node->next, &next, newnode)) {
		if (next != NULL) next->prev = newnode;
		return RESULT_TRUE;
	}
	free(newnode);

	return RESULT_RETRY;
}
result_t finish_remove(key_t k, node_t *node, void *node_val) {
	// Not present: can't remove it.
	if (node->k != k || node_val == NULL)
		return RESULT_FALSE;

	// Attempt to mark as deleted.
	if (atomic_compare_exchange_strong(&node->v, &node_val, NULL)) {
		return RESULT_TRUE;
	}

	return RESULT_RETRY;
}

// help_remove is given two nodes, pred is an undeleted node such that
// pred.next = node, and node is a physically deleted node. A marker node
// is then appended to node, and finally both node and its marker are removed
// from the list. Whichever thread removed them is responsible for claiming
// them to be garbage collected.
void help_remove(node_t *pred, node_t *node) {
	if (node->v != node || node->marked)
		return;

	int canfree = 1;
	node_t *n = node->next;
	node_t *newnode = new_node(node, n, 0, NULL);
	while (n == NULL || !n->marked) {
		newnode->next = n;
		newnode->v = newnode;
		newnode->marked = 1;
		if (atomic_compare_exchange_strong(&node->next, &n, newnode))
			canfree = 0;
		n = node->next;
	}
	if (canfree) {
		free(newnode);
	}
	if (pred->next != node || pred->marked)
		return;
	if (atomic_compare_exchange_strong(&pred->next, &node, n->next)) {
		// We removed both: add them to our free list.
		gc_defer(node);
		gc_defer(n);
	}

	// Update the prev pointer inexactly
	node_t *pred_next = pred->next;
	if (NULL != pred_next)
		pred_next->prev = pred;
}

// try_mark_phys_remove attempts to mark a logically deleted node for physical
// removal. This should only be called by the background thread.
void try_mark_phys_remove(node_t *node) {
	void *expected = NULL;
	atomic_compare_exchange_strong(&node->v, &expected, node);
}

// set_init allocates a new set to be used. num_threads is the number of threads
// which will be accessing the data structure (excluding the background thread).
// set_init also starts the background thread and initialises URCU.
//
// NOT THREAD SAFE. Use before starting threads.
intset_t *set_init(int num_threads) {
	intset_t *set = malloc(sizeof(intset_t));
	if (NULL == set) {
		perror("malloc");
		exit(1);
	}

	node_t *min = new_node(NULL, NULL, KEY_MIN, NULL);
	set->head = min;

	// Simplest starting index.
	set->idx = new_idx(1);
	set->idx->size = 1;
	set->idx->elems[0] = (idx_elem_t) {
		.k = KEY_MIN, .node = min
	};

	urcu_init(num_threads+1);
	gc_init(num_threads+1);
	bg_start(set, num_threads);

	return set;
}

// set_thread_register makes a thread known to the set. It should be called before
// starting any abstract operations. Ids should be in the range [0, num_threads).
void set_thread_register(int id) {
	urcu_register(id);
	gc_register(id);
}

// set_thread_unregister clears up any of the thread's leftover resources.
void set_thread_unregister(void) {
	urcu_unregister();
}

// set_destroy stops the background thread, and then clears all resources associated
// with the set.
//
// NOT THREAD SAFE. Use after stopping all threads.
void set_destroy(intset_t *set) {
	node_t *prev, *curr;

	bg_stop();
	urcu_teardown();
	gc_teardown();

	curr = set->head;
	while (NULL != curr) {
		prev = curr;
		curr = curr->next;
		free(prev);
	}

	free(set->idx);
	free(set);
}

// Get the size of the set, excluding the head node, and ignoring
// logically and physically deleted nodes.
int set_size(intset_t *set) {
	int size = 0;
	node_t *curr = set->head->next;
	while (curr != NULL) {
		if (curr->v != NULL && curr->v != curr)
			size++;
		curr = curr->next;
	}
	return size;
}

// new_node allocates a new node with the given parameters.
node_t *new_node(node_t *prev, node_t *next, key_t k, void *v) {
	node_t *node = calloc(sizeof(node_t), 1);
	if (NULL == node) {
		perror("malloc");
		exit(1);
	}
	node->prev = prev;
	node->next = next;
	node->k = k;
	node->v = v;
	return node;
}

// new_idx allocates a new index with the given capacity. It returns
// an uninitialised elems array, and a size of 0.
idx_t *new_idx(int cap) {
	idx_t *idx = malloc(sizeof(idx_t));
	if (idx == NULL) {
		perror("malloc");
		exit(1);
	}
	idx_elem_t *elems = malloc(sizeof(idx_elem_t) * cap);
	if (elems == NULL) {
		perror("malloc");
		exit(1);
	}

	idx->cap = cap;
	idx->size = 0;
	idx->elems = elems;
	return idx;
}

void free_idx(idx_t *idx) {
	free(idx->elems);
	free(idx);
}

// For debugging - use only on small lists!
void set_print(intset_t *set) {
	node_t *curr = set->head;
	while (curr != NULL) {
		printf("%d", curr->k);
		
		// Logically deleted?
		if (curr->v == NULL)
			printf("!");
		
		// Physically deleted?
		if (curr->v == curr) {
			if (curr->marked)
				printf("MMMMM");
			else
				printf("@@@@@");
		}
		
		if (curr->next != NULL)
			printf(" -> ");
		curr = curr->next;
	}
	printf("\n");
}
