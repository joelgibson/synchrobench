#define ALGONAME "Universal construction linked list"

struct node {
  val_t val;
  struct node *next;
};

struct intset {
  _Atomic(node_t *) head;
};
