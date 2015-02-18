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

typedef enum {
  OP_CONTAINS,
  OP_INSERT,
  OP_REMOVE,
} op_t;

typedef enum {
  RESULT_RETRY,
  RESULT_FALSE,
  RESULT_TRUE,
} result_t;

int do_operation(intset_t *set, op_t op, key_t k, void *v, int fast);
result_t finish_contains(key_t k, node_t *node, void *node_val);
result_t finish_insert(key_t k, void *v, node_t *node, void *node_val, node_t *next);
result_t finish_remove(key_t k, node_t *node, void *node_val);

// Interface functions
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

/* use_idx returns the last element in the index with key less than or equal
 * to the given key.
 */
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

/* search locates a pair of nodes (node, next) in the list satisfying the
 * following conditions:
 * - They are either adjacent, or node is the end of the list and next is null.
 * - node.k <= k < next.k (if next is null, only node.k <= k)
 * - Both node and next are not physically deleted.
 *
 * After this search is complete, the relevant operation is tried: if it succeeds,
 * we finish, otherwise we retry the search from where we left off.
 */
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
      if (op == OP_CONTAINS)    result = finish_contains(k, node, node_val);
      else if (op == OP_INSERT) result = finish_insert(k, v, node, node_val, next);
      else /*op == OP_REMOVE*/  result = finish_remove(k, node, node_val);

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

// Mark a node to be physically removed - it will be removed during a search.
// Should only be called by the background thread.
void try_mark_phys_remove(node_t *node) {
  void *expected = NULL;
  atomic_compare_exchange_strong(&node->v, &expected, node);
}


// Create a new set.
intset_t *set_new(int num_threads) {
  intset_t *set = malloc(sizeof(intset_t));
  if (NULL == set) {
    perror("malloc");
    exit(1);
  }

  node_t *min = new_node(NULL, NULL, KEY_MIN, NULL);
  set->head = min;

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

// Delete the set and all its elements.
void set_delete(intset_t *set) {
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

// Get the size of the set, excluding the two head and
// tail sentinels.
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

// For debugging

void set_print(intset_t *set) {
  node_t *curr = set->head;
  while (curr != NULL) {
    printf("%d", curr->k);
    if (curr->v == NULL) printf("!");
    if (curr->v == curr) printf("@@@@@");
    if (curr->next != NULL)
      printf(" -> ");
    curr = curr->next;
  }
  printf("\n");
}

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
