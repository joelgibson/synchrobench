#include "intset.h"
#include "universal.h"

// Import common set operations
#include "mixin.c"

// Creates a copy of the given list
static node_t *copy_list(node_t *head) {
  node_t *newlist = NULL;
  node_t **loc = &newlist;
  for (node_t *cur = head; cur != NULL; cur = cur->next) {
    *loc = new_node(cur->val, NULL);
    loc = &((*loc)->next);
  }
  return newlist;
}

// Returns the first node n such that n.val >= val. If outPrev is not null, will
// return the predecessor of n, which is strictly smaller than val.
static inline node_t *find(node_t *head, val_t val, node_t **outPrev) {
  node_t *curr, *prev;
  prev = head;
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
  node_t *head = atomic_load(&set->head);
  node_t *node = find(head, val, NULL);
  return node->val == val;
}

int set_insert(intset_t *set, val_t val) {
  node_t *head, *newhead, *curr, *prev, *newnode;
  for (;;) {
    head = atomic_load(&set->head);
    newhead = copy_list(head);
    curr = find(newhead, val, &prev);
    if (curr->val == val)
      return 0;
    newnode = new_node(val, curr);
    prev->next = newnode;
    if (atomic_compare_exchange_strong(&set->head, &head, newhead))
      return 1;
  }
}

int set_remove(intset_t *set, val_t val) {
  node_t *head, *newhead, *curr, *prev;
  for (;;) {
    head = atomic_load(&set->head);
    newhead = copy_list(head);
    curr = find(newhead, val, &prev);
    if (curr->val != val)
      return 0;
    prev->next = curr->next;
    if (atomic_compare_exchange_strong(&set->head, &head, newhead))
      return 1;
  }
}
