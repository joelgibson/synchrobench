#define ALGONAME "Hand-over-hand locking linked list"

struct node {
  val_t val;
  struct node *next;
  ptlock_t lock;
};

struct intset {
  node_t *head;
};
