#define ALGONAME "Harris linked list"

struct node {
  val_t val;
  _Atomic(struct node *) next;
};

struct intset {
  node_t *head;
};
