#include <limits.h>

// What proportion of the list is indexed
#define IDX_GAP (4)

typedef int key_t;
#define KEY_MIN INT_MIN
#define KEY_MAX INT_MAX

typedef struct node {
  key_t k;
  int marked;
  _Atomic(void *) v;
  _Atomic(struct node *) next;
  struct node * prev;

  // For use in garbage collection only
  struct node * gcnext;
} node_t;

typedef struct idx_elem {
  key_t k;
  node_t *node;
} idx_elem_t;

typedef struct idx {
  int size, cap;
  idx_elem_t *elems;
} idx_t;

typedef struct intset {
  node_t *head;
  idx_t *idx;
} intset_t;

struct bg_arg {
  intset_t *set;
  int num_threads;
};
intset_t *set_new(int num_threads);
void set_delete(intset_t *set);
int set_size(intset_t *set);
void set_print(intset_t *set);
int set_contains(intset_t *set, key_t key);
int set_insert(intset_t *set, key_t key);
int set_remove(intset_t *set, key_t key);
void set_scanall(intset_t *set);

node_t *new_node(node_t *prev, node_t *next, key_t k, void *v);

void help_remove(node_t *pred, node_t *node);
void try_mark_phys_remove(node_t *node);

idx_t *new_idx(int size);
node_t *use_idx(intset_t *set, key_t k);
void free_idx(idx_t *idx);
