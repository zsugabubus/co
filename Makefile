all : co

co : co.c Makefile
	$(CC) -g -std=gnu11 -Wall -Wextra -o $@ $<

run : co
	gdb $< -ex run

.PHONY : all
