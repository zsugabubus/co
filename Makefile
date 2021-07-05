all : co

CFLAGS += -O3 -g -std=gnu11 -Wall -Wextra

# CFLAGS += -D'CO_HAVE_VALGRIND'
# LDFLAGS += $(shell pkg-config --libs --cflags valgrind)

co : t.c co.* Makefile
	$(CC) -DNDEBUG -DCO_NDEBUG $(CFLAGS) $(LDFLAGS) -o $@ $< co.c

run : co
	gdb $< -ex 'set confirm no' -ex run

.PHONY : all
