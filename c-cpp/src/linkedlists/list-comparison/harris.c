#include "intset.h"
#include "harris.h"

// Import common set operations
#include "mixin.c"

// The following functions deal with the marked pointers.
static inline int is_marked_ref(node_t *node) {
	uintptr_t val = (uintptr_t) node;
	return (int) (val & (uintptr_t)1);
}
static inline node_t *get_unmarked_ref(node_t *node) {
	uintptr_t val = (uintptr_t) node;
	return (node_t *) (val & ~(uintptr_t)1);
}
static inline node_t *get_marked_ref(node_t *node) {
	uintptr_t val = (uintptr_t) node;
	return (node_t *) (val | (uintptr_t)1);
}

/*
 * harris_search looks for value val, it
 *  - returns right_node owning val (if present) or its immediately higher 
 *    value present in the list (otherwise) and 
 *  - sets the left_node to the node owning the value immediately lower than val. 
 * Encountered nodes that are marked as logically deleted are physically removed
 * from the list, yet not garbage collected.
 */
static node_t *harris_search(intset_t *set, val_t val, node_t **left_node) {
	node_t *left_node_next, *right_node;
	left_node_next = set->head;
	
search_again:
	do {
		node_t *t = set->head;
		node_t *t_next = atomic_load(&set->head->next);
		
		/* Find left_node and right_node */
		do {
			if (!is_marked_ref(t_next)) {
				(*left_node) = t;
				left_node_next = t_next;
			}
			t = get_unmarked_ref(t_next);
			if (NULL == atomic_load(&t->next))
				break;
			t_next = atomic_load(&t->next);
		} while (is_marked_ref(t_next) || (t->val < val));
		right_node = t;
		
		/* Check that nodes are adjacent */
		if (left_node_next == right_node) {
			node_t *right_node_next = atomic_load(&right_node->next);
			if (right_node_next != NULL && is_marked_ref(right_node_next))
				goto search_again;
			else return right_node;
		}
		
		/* Remove one or more marked nodes */
		if (atomic_compare_exchange_strong(&(*left_node)->next, &left_node_next, right_node)) {
			node_t *right_node_next = atomic_load(&right_node->next);
			if (right_node_next != NULL && is_marked_ref(right_node_next))
				goto search_again;
			else return right_node;
		} 
		
	} while (1);
}

/*
 * harris_find returns whether there is a node in the list owning value val.
 */
int set_contains(intset_t *set, val_t val) {
	node_t *right_node, *left_node;
	left_node = set->head;
	
	right_node = harris_search(set, val, &left_node);
	if ((!right_node->next) || right_node->val != val)
		return 0;
	else 
		return 1;
}

/*
 * harris_find inserts a new node with the given value val in the list
 * (if the value was absent) or does nothing (if the value is already present).
 */
int set_insert(intset_t *set, val_t val) {
	node_t *newnode, *right_node, *left_node;
	left_node = set->head;
	
	do {
		right_node = harris_search(set, val, &left_node);
		if (right_node->val == val)
			return 0;
		newnode = new_node(val, right_node);
		if (atomic_compare_exchange_strong(&left_node->next, &right_node, newnode))
			return 1;
	} while(1);
}

/*
 * harris_find deletes a node with the given value val (if the value is present) 
 * or does nothing (if the value is already present).
 * The deletion is logical and consists of setting the node mark bit to 1.
 */
int set_remove(intset_t *set, val_t val) {
	node_t *right_node, *right_node_next, *left_node;
	
	do {
		right_node = harris_search(set, val, &left_node);
		if (right_node->val != val)
			return 0;
		right_node_next = atomic_load(&right_node->next);
		if (!is_marked_ref(right_node_next)) {
			if (atomic_compare_exchange_strong(
					&right_node->next,
					&right_node_next,
					get_marked_ref(right_node_next)))
				break;
		}
	} while(1);
	node_t *expected = right_node;
	if (!atomic_compare_exchange_strong(&left_node->next, &expected, right_node_next))
		right_node = harris_search(set, right_node->val, &left_node);
	return 1;
}
