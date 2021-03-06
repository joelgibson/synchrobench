// Create a new set.
intset_t *set_new(void) {
  intset_t *set = malloc(sizeof(intset_t));
  if (NULL == set) {
    perror("malloc");
    exit(1);
  }

  node_t *max = new_node(VAL_MAX, NULL);
  node_t *min = new_node(VAL_MIN, max);
  set->head = min;
  return set;
}

// Delete the set and all its elements.
void set_delete(intset_t *set) {
  node_t *prev, *curr;

  curr = set->head;
  while (NULL != curr) {
    prev = curr;
    curr = curr->next;
    DESTROY_LOCK(&prev->lock);
    free(prev);
  }
  free(set);
}

// Get the size of the set, excluding the two head and
// tail sentinels.
int set_size(intset_t *set) {
  int size = 0;
  node_t *curr = set->head->next;
  while (curr->next != NULL) {
    size++;
    curr = curr->next;
  }
  return size;
}

node_t *new_node(val_t val, node_t *next) {
  node_t *node = calloc(sizeof(node_t), 1);
  if (NULL == node) {
    perror("malloc");
    exit(1);
  }
  node->val = val;
  node->next = next;
  INIT_LOCK(&node->lock);
  return node;
}

// For debugging
void set_print(intset_t *set) {
  node_t *curr = set->head;
  while (curr != NULL) {
    printf("%d", curr->val);
    if (curr->next != NULL)
      printf(" -> ");
    curr = curr->next;
  }
  printf("\n");
}
