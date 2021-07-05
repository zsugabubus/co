all : co

CFLAGS += -O1 -g -std=gnu11 -Wall -Wextra

# CFLAGS += -D'CO_HAVE_VALGRIND'
# LDFLAGS += $(shell pkg-config --libs --cflags valgrind)

co : t.c co.c co.h config.h Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

run : co
	gdb $< -ex 'set confirm no' -ex run

.PHONY : all
