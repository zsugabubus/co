#include <alloca.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

#if CO_HAVE_VALGRIND
# include <valgrind.h>
#endif

#include "co.h"

#define CO_STACK_GROWSDOWN (CO_STACK_SIZE(0, 1) < 0)

static __attribute__((naked))
void
co_trampoline(void)
{
	/**
	 * Waking up a coroutine for the first time requires special handling,
	 * because we must transform the return from co_switch() to be a call
	 * to routine() with the following arguments:
	 *
	 *   1. Coroutine self reference. It is on the stack.
	 *
	 *   2. Return value of the first co_switch(they, me, arg).
	 */
#ifdef __x86_64__
	__asm__ volatile(
			"pop %%rdi\n\t"
			"mov %%rax,%%rsi\n\t"
			"ret\n\t"
		::
	);
#endif
	__builtin_unreachable();
}

int
co_alloc_stack(Coroutine *c, size_t stack_sz)
{
	void *p = mmap(NULL, stack_sz,
			PROT_READ | PROT_WRITE,
#ifdef MAP_STACK
			MAP_STACK |
#endif
#ifdef MAP_GROWSDOWN
			(CO_STACK_GROWSDOWN ? MAP_GROWSDOWN : 0) |
#endif
			MAP_ANONYMOUS |
			MAP_PRIVATE,
			-1, 0);
	if (MAP_FAILED == p)
		return -1;

	c->co_stack = p + (CO_STACK_GROWSDOWN ? stack_sz : 0);
	c->co_stack_sz = stack_sz;
	c->co_frame = c->co_stack;

	return 0;
}

void
co_free_stack(Coroutine *c)
{
	int ret = munmap((char *)c->co_stack - (CO_STACK_GROWSDOWN ? c->co_stack_sz : 0), c->co_stack_sz);
	assert(0 <= ret);
}

#define CO_INCLUDE_FILE "co.c.inc"
#include "co.inc"
