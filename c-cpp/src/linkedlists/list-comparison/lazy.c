#include "intset.h"
#include "lazy.h"

#include "mixin_lockednode.c"

// Check both curr and pred are both unmarked and that pred.next == curr
// to verify that the entries and adjacent and present in the list.
static inline int validate(node_t *pred, node_t *curr) {
  return !pred->marked && !curr->marked && pred->next == curr;
}

int set_contains(intset_t *set, val_t val) {
  node_t *curr;
  curr = set->head;
  while (curr->val < val)
    curr = curr->next;
  return curr->val == val && !curr->marked;
}

int set_insert(intset_t *set, val_t val) {
  node_t *curr, *pred, *newnode;
  int result;

  pred = set->head;
  curr = pred->next;
  while (curr->val < val) {
    pred = curr;
    curr = curr->next;
  }

  LOCK(&pred->lock);
  LOCK(&curr->lock);
  result = validate(pred, curr) && curr->val != val;
  if (result) {
    newnode = new_node(val, curr);
    pred->next = newnode;
  }
  UNLOCK(&pred->lock);
  UNLOCK(&curr->lock);
  return result;
}

int set_remove(intset_t *set, val_t val) {
  node_t *curr, *pred;
  int result;

  pred = set->head;
  curr = pred->next;
  while (curr->val < val) {
    pred = curr;
    curr = curr->next;
  }

  LOCK(&pred->lock);
  LOCK(&curr->lock);
  result = validate(pred, curr) && curr->val == val;
  if (result) {
    curr->marked = 1;
    pred->next = curr->next;
  }
  UNLOCK(&pred->lock);
  UNLOCK(&curr->lock);
  return result;
}
