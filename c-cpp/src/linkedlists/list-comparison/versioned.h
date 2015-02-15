#define ALGONAME "Versioned linked list"

// Size of the number holding the versioned lock.
typedef uint32_t vlock_t;

struct node {
  val_t val;
  struct node *next;
  int deleted;
  _Atomic(vlock_t) vlock;
};

struct intset {
  node_t *head;
};
