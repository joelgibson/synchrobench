#include "intset.h"
#include "versioned.h"

// Import common set operations
#include "mixin.c"

// The versioned lock is an int. The LSB represents the locking
// state (0=unlocked, 1=locked), and the rest of the number is
// the lock version.
static inline int get_version(node_t *node) {
  vlock_t val = atomic_load(&node->vlock);
  return val & ~((vlock_t)1);
}
static inline int try_lock_at_version(node_t *node, vlock_t ver) {
  return atomic_compare_exchange_strong(&node->vlock, &ver, ver+1);
}
static inline void lock_at_current_version(node_t *node) {
  int success = 0;
  while (!success) {
    vlock_t ver = get_version(node);
    success = atomic_compare_exchange_strong(&node->vlock, &ver, ver+1);
  }
}
static inline void unlock_and_increment(node_t *node) {
  vlock_t val = atomic_load(&node->vlock);
  atomic_store(&node->vlock, val+1);
}

// This is a short traversal, starting from prev, which will
// continue until outPrev < val <= outCurr, and outPrev is not
// deleted, and return 1. If prev was found to be deleted at some
// point, it will return 0 and none of the out parameters will
// be set.
static int validate(val_t val, node_t *prev,
    node_t **outPrev, vlock_t *outVer, node_t **outCurr) {
  vlock_t pVer;
  node_t *curr;

retry_validate:
  pVer = get_version(prev);
  if (prev->deleted)
    return 0;
  curr = prev->next;
  while (curr->val < val) {
    pVer = get_version(curr);
    if (curr->deleted)
      goto retry_validate;
    prev = curr;
    curr = curr->next;
  }

  *outPrev = prev;
  *outVer = pVer;
  *outCurr = curr;
  return 1;
}

// Returns the last node strictly less than val.
static node_t *waitfree_traversal(intset_t *set, val_t val) {
  node_t *prev = set->head, *curr = set->head;
  while (curr->val < val) {
    prev = curr;
    curr = curr->next;
  }
  return prev;
}

int set_contains(intset_t *set, val_t val) {
  node_t *curr = set->head;
  while (curr->val < val)
    curr = curr->next;
  return curr->val == val && !curr->deleted;
}

int set_insert(intset_t *set, val_t val) {
  node_t *prev, *curr, *newnode;
  vlock_t pVer;
  newnode = new_node(val, NULL);

retry_insert_full:
  prev = waitfree_traversal(set, val);
retry_insert_partial:
  if (!validate(val, prev, &prev, &pVer, &curr))
    goto retry_insert_full;
  if (curr->deleted)
    goto retry_insert_partial;
  if (curr->val == val)
    return 0;

  newnode->next = curr;
  if (!try_lock_at_version(prev, pVer))
    goto retry_insert_partial;
  
  prev->next = newnode;
  unlock_and_increment(prev);
  return 1;
}

int set_remove(intset_t *set, val_t val) {
  node_t *prev, *curr;
  vlock_t pVer;

retry_remove_full:
  prev = waitfree_traversal(set, val);
retry_remove_partial:
	if (!validate(val, prev, &prev, &pVer, &curr))
		goto retry_remove_full;
	if (curr->val != val || curr->deleted)
		return 0;
	if (!try_lock_at_version(prev, pVer))
		goto retry_remove_partial;
	
	lock_at_current_version(curr);
	curr->deleted = 1;
	prev->next = curr->next;
  unlock_and_increment(prev);
  unlock_and_increment(curr);
  return 1;
}
