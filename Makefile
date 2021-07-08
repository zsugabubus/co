all : check

PERF := perf stat
ifeq ($(CI),true)
	PERF := # Nothing.
endif

check :
	set -eu; \
	for O in -O3 -O0; do \
	for fp in '-DCO_HAVE_FRAMEPOINTER=0 -fomit-frame-pointer' '-DCO_HAVE_FRAMEPOINTER=1 -fno-omit-frame-pointer'; do \
	for rz in '-DCO_HAVE_REDZONE=0 -mno-red-zone' '-DCO_HAVE_REDZONE=1 -mred-zone'; do \
		CFLAGS="$$O $$fp $$rz" $(MAKE) check-one; \
	done; \
	done; \
	done; \
	$(MAKE) clean

run :
	gdb check -ex 'set confirm no' -ex 'set layout asm' -ex run

check-one :
	$(CC) $(CFLAGS)	-o check -w -g -std=gnu11 t.c co.c
	./check
	@# Perf does not return failure when process terminated by a signal.
	$(PERF) ./check

clean :
	$(RM) check

.PHONY : all check check-one clean
