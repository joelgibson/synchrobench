#define ALGONAME "Lazy linked list"

struct node {
  val_t val;
  struct node *next;
  ptlock_t lock;
  int marked;
};

struct intset {
  node_t *head;
};
