ROOT = ../../..

include $(ROOT)/common/Makefile.common

ifeq ($(STM),SEQUENTIAL)
  BINS = $(BINDIR)/sequential-linkedlist
else ifeq ($(STM),LOCKFREE)
  BINS = $(BINDIR)/lockfree-linkedlist
else
  BINS = $(BINDIR)/$(STM)-linkedlist
endif

CFLAGS += -Wall -pedantic -std=gnu11
#LDFLAGS += -ltcmalloc

.PHONY:	all clean

all: sequential harris versioned fomitchev lazy coupling universal fr-waitfree-contains

sequential: sequential.o test.c
	$(CC) $(CFLAGS) -DSEQUENTIAL sequential.o test.c -o sequential $(LDFLAGS)

harris: harris.o test.c
	$(CC) $(CFLAGS) -DHARRIS harris.o test.c -o harris $(LDFLAGS)

versioned: versioned.o test.c
	$(CC) $(CFLAGS) -DVERSIONED versioned.o test.c -o versioned $(LDFLAGS)

fomitchev: fomitchev.o test.c
	$(CC) $(CFLAGS) -DFOMITCHEV fomitchev.o test.c -o fomitchev $(LDFLAGS)

fr-waitfree-contains: fr-waitfree-contains.o test.c
	$(CC) $(CFLAGS) -DFR_WAITFREE_CONTAINS fr-waitfree-contains.o test.c -o fr-waitfree-contains $(LDFLAGS)

lazy: lazy.o test.c
	$(CC) $(CFLAGS) -DLAZY lazy.o test.c -o lazy $(LDFLAGS)

coupling: coupling.o test.c
	$(CC) $(CFLAGS) -DCOUPLING coupling.o test.c -o coupling $(LDFLAGS)

universal: universal.o test.c
	$(CC) $(CFLAGS) -DUNIVERSAL universal.o test.c -o universal $(LDFLAGS)

clean:
	-rm -f *.o sequential harris versioned fomitchev lazy coupling universal fr-waitfree-contains
