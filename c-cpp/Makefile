.PHONY:	all

BENCHS = src/trees/sftree src/linkedlists/lockfree-list src/hashtables/lockfree-ht src/trees/rbtree src/skiplists/sequential
LBENCHS = src/trees/tree-lock src/linkedlists/lock-coupling-list src/linkedlists/lazy-list src/hashtables/lockbased-ht src/skiplists/skiplist-lock 
LFBENCHS = src/trees/lfbstree src/linkedlists/lockfree-list src/hashtables/lockfree-ht src/skiplists/rotating src/skiplists/fraser src/skiplists/nohotspot src/skiplists/arridx src/skiplists/fraser-mod

#MAKEFLAGS+=-j4

MALLOC=TC

.PHONY:	clean all $(BENCHS) $(LBENCHS)

all:	lock spinlock lockfree estm sequential

lock: clean-build
	$(MAKE) "LOCK=MUTEX" $(LBENCHS)

spinlock: clean-build
	$(MAKE) "LOCK=SPIN" $(LBENCHS)

sequential: clean-build
	$(MAKE) "STM=SEQUENTIAL" $(BENCHS)

lockfree: clean-build
	for dir in $(LFBENCHS); do \
	$(MAKE) "STM=LOCKFREE" -C $$dir; \
	done

estm: clean-build
	$(MAKE) -C src/utils/estm-0.3.0
	$(MAKE) "STM=ESTM" $(BENCHS)

clean-build: 
	rm -rf build

clean:
	$(MAKE) -C src/utils/estm-0.3.0 clean
	rm -rf build bin

$(BENCHS):
	$(MAKE) -C $@ $(TARGET)

$(LBENCHS):
	$(MAKE) -C $@ $(TARGET)
