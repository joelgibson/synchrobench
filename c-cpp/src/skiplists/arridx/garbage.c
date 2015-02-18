#include <stdlib.h>
#include <stdatomic.h>

#include "skiplist.h"
#include "garbage.h"

/* Garbage collection: there is one global list of nodes to be garbage
 * collected. In addition, each thread maintains its own list, and when
 * that grows too large, it attempts to splice that into the main list.
 */

// Number of threads participating in garbage collection.
int gc_nthreads;

// Global list of nodes to be collected.
_Atomic(node_t *) gc_head;

// Array of per-thread information.
gc_node_t *gc_nodes;

// Thread variable: pointer into above array.
__thread gc_node_t *gc_node;

// Called once to initialise the garbage collection lists.
void gc_init(int nthreads) {
  gc_nthreads = nthreads;
  gc_nodes = calloc(nthreads, sizeof(gc_node_t));
}

void gc_teardown(void) {
  // Free any leftover nodes
  for (int i = 0; i < gc_nthreads; i++)
    gc_freeall(gc_nodes[i].head);
  gc_freeall(gc_head);
  
  // Free the lists
  free(gc_nodes);
}

// Called once per thread, with a unique id in the range [0, nthreads).
void gc_register(int tid) {
  gc_node = &gc_nodes[tid];
}

// Defer the collection of this node for later.
void gc_defer(node_t *node) {
  node->gcnext = gc_node->head;
  gc_node->head = node;
  if (gc_node->tail == NULL)
    gc_node->tail = node;
  gc_node->count++;

  // If we have too many, attempt to splice into global freelist.
  int thres = 10;
  if (gc_node->count >= thres) {
    //if (gc_node->count > thres)
    //  printf("OVER, at %d\n", gc_node->count);
    node_t *head = atomic_load(&gc_head);
    gc_node->tail->gcnext = head;
    if (atomic_compare_exchange_strong(&gc_head, &head, gc_node->head)) {
      gc_node->head = gc_node->tail = NULL;
      gc_node->count = 0;
    }
  }
}

// Claim any deferred nodes and return them to the caller.
node_t *gc_cut(void) {
  for (;;) {
    node_t *head = atomic_load(&gc_head);
    if (atomic_compare_exchange_strong(&gc_head, &head, NULL))
      return head;
  }
}

// Free the given list.
void gc_freeall(node_t *head) {
  while (head != NULL) {
    node_t *next = head->gcnext;
    free(head);
    head = next;
  }
}
