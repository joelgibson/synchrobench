#include "intset.h"
#include "sequential.h"

// Import common set operations.
#include "mixin.c"

// Returns the first node n such that n.val >= val. If outPrev is not null, will
// return the predecessor of n, which is strictly smaller than val.
static inline node_t *find(intset_t *set, val_t val, node_t **outPrev) {
  node_t *curr, *prev;
  prev = set->head;
  curr = prev->next;

  while (curr->val < val) {
    prev = curr;
    curr = curr->next;
  }

  if (outPrev != NULL)
    *outPrev = prev;

  return curr;
}

int set_contains(intset_t *set, val_t val) {
  node_t *node = find(set, val, NULL);
  return node->val == val;
}

int set_insert(intset_t *set, val_t val) {
  node_t *curr, *prev, *newnode;
  curr = find(set, val, &prev);

  if (curr->val == val)
    return 0;

  newnode = new_node(val, curr);
  prev->next = newnode;
  return 1;
}

int set_remove(intset_t *set, val_t val) {
  node_t *curr, *prev;
  curr = find(set, val, &prev);
  
  if (curr->val != val)
    return 0;

  prev->next = curr->next;
  return 1;
}
