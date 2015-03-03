ROOT = ../../..

include $(ROOT)/common/Makefile.common

BINS = $(BINDIR)/lockfree-arridx

CFLAGS += -Wall -pedantic -std=gnu11

CC=gcc

.PHONY:	all clean

all: main

main: skiplist.c test.c urcu.c garbage.c background.c
	$(CC) $(CFLAGS) skiplist.c test.c urcu.c garbage.c background.c -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
