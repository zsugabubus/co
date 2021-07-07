all : check

check :
	set -eu; \
	for O in -O0 -Os -O3; do \
	for fp in '-DCO_SAVE_BP=1 -fno-omit-frame-pointer' '-DCO_SAVE_BP=0 -fomit-frame-pointer'; do \
		CFLAGS="$$O $$fp" $(MAKE) check-one; \
	done; \
	done; \
	$(MAKE) clean

run :
	gdb check -ex 'set confirm no' -ex 'set layout asm' -ex run

check-one :
	$(CC) $(CFLAGS)	-o check -w -g -std=gnu11 t.c co.c
	perf stat ./check

clean :
	$(RM) check

.PHONY : all check check-one clean
