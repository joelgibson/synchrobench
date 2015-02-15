#define ALGONAME "Sequential linked list"

struct node {
  val_t val;
  struct node *next;
};

struct intset {
  node_t *head;
};
