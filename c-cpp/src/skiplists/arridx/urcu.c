#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdatomic.h>

#include "urcu.h"

// Globals
int urcu_nthreads;
rcu_node *urcu_table;

// Thread-locals
__thread unsigned long *urcu_times;
_Atomic(unsigned long) __thread *urcu_time;

// Called once at application startup, with the maximum number
// threads which will be participating.
void urcu_init(int nthreads) {
	urcu_nthreads = nthreads;
	urcu_table = calloc(nthreads, sizeof(rcu_node));
	if (urcu_table == NULL) {
		perror("malloc");
		exit(1);
	}
	for (int i = 0; i < nthreads; i++) {
		urcu_table[i].time = 1;
	}
	printf("initializing URCU finished, node_size: %zd\n", sizeof(rcu_node));
}

void urcu_teardown(void) {
	free(urcu_table);
}

// Called once per thread, with a unique id in the range [0, nthreads).
void urcu_register(int id) {
	urcu_times = calloc(urcu_nthreads, sizeof(unsigned long));
	if (urcu_times == NULL) {
		perror("malloc");
		exit(1);
	}
	urcu_time = &urcu_table[id].time;
}

// Called to teardown any thread-local resources.
void urcu_unregister(void) {
	free(urcu_times);
}

// Enter read-side-critical section.
void urcu_read_lock(void) {
	atomic_fetch_add(urcu_time, 1);
}

// Exit read-side-critical section.
void urcu_read_unlock(void) {
	atomic_fetch_add(urcu_time, 1);
}

// Block until every thread currently in a read-side critical section
// has exited that read-side critical section.
void urcu_synchronize(void) {
	for (int i = 0; i < urcu_nthreads; i++)
		urcu_times[i] = atomic_load(&urcu_table[i].time);

	for (int i = 0; i < urcu_nthreads; i++) {
		if (urcu_times[i] & 1) continue;
		for (;;) {
			unsigned long t = atomic_load(&urcu_table[i].time);
			if ((t&1) || (t > urcu_times[i])) break;
		}
	}
}
