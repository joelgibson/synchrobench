// How many deferred nodes a thread stores before
// attempting to give them to the background thread.
#define GC_THRES (10)

// Garbage collection
void gc_init(int nthreads);
void gc_teardown(void);
void gc_register(int tid);
void gc_defer(node_t *node);
node_t *gc_cut(void);
void gc_free_list(node_t *head);

typedef struct {
	// Freelist
	node_t *head, *tail;
	int count;

	// Padding for cache lines
	char pad[80];
} gc_node_t;
