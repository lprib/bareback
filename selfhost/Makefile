CFLAGS = -fno-strict-aliasing -O0 -g -ggdb

.PHONY: all test
all: lisp test

lisp: lisp.c
	$(CC) $(CFLAGS) $^ -o $@

test:
	./test.sh
