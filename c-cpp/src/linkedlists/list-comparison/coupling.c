#include "intset.h"
#include "lazy.h"

#include "mixin_lockednode.c"

int set_contains(intset_t *set, val_t val) {
  node_t *curr, *next;
  int found;

  curr = set->head;
  LOCK(&curr->lock);
  next = curr->next;
  LOCK(&next->lock);

  while (next->val < val) {
    UNLOCK(&curr->lock);
    curr = next;
    next = curr->next;
    LOCK(&next->lock);
  }

  found = val == next->val;
  UNLOCK(&curr->lock);
  UNLOCK(&next->lock);
  return found;
}

int set_insert(intset_t *set, val_t val) {
  node_t *curr, *next, *newnode;
  int found;

  curr = set->head;
  LOCK(&curr->lock);
  next = curr->next;
  LOCK(&next->lock);

  while (next->val < val) {
    UNLOCK(&curr->lock);
    curr = next;
    next = curr->next;
    LOCK(&next->lock);
  }

  found = val == next->val;
  if (!found) {
    newnode = new_node(val, next);
    curr->next = newnode;
  }
  UNLOCK(&curr->lock);
  UNLOCK(&next->lock);
  return !found;
}

int set_remove(intset_t *set, val_t val) {
  node_t *curr, *next;
  int found;

  curr = set->head;
  LOCK(&curr->lock);
  next = curr->next;
  LOCK(&next->lock);

  while (next->val < val) {
    UNLOCK(&curr->lock);
    curr = next;
    next = curr->next;
    LOCK(&next->lock);
  }

  found = val == next->val;
  if (found) {
    curr->next = next->next;
  }
  UNLOCK(&curr->lock);
  UNLOCK(&next->lock);
  if (found) {
    DESTROY_LOCK(&next->lock);
    free(next);
  }
  return found;
}
