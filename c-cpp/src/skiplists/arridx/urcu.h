typedef struct rcu_node_t {
    _Atomic(unsigned long) time; 
    char p[184];
} rcu_node;

void urcu_init(int num_threads);
void urcu_teardown(void);
void urcu_read_lock(void);
void urcu_read_unlock(void);
void urcu_synchronize(void); 
void urcu_register(int id);
void urcu_unregister(void);
