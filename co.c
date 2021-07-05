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
	__asm__(
			"pop %%rdi\n\t"
			"mov %%rax,%%rsi\n\t"
			"ret\n\t"
		::
	);
#endif
	__builtin_unreachable();
}

static void
co_return(void);

int
co_alloc_stack(Coroutine *c, size_t stack_sz)
{
	void *p = mmap(NULL, stack_sz, PROT_READ|PROT_WRITE,
#ifdef MAP_STACK
			MAP_STACK |
#endif
#ifdef MAP_GROWSDOWN
			MAP_GROWSDOWN |
#endif
			MAP_ANONYMOUS |
			MAP_PRIVATE, -1, 0);
	if (MAP_FAILED == p)
		return -1;

#ifdef __x86_64__
	c->co_stack = (char *)p + stack_sz;
#endif
	c->co_stack_sz = stack_sz;

	c->co_frame =c->co_stack;

	return 0;
}

void
co_free_stack(Coroutine *c)
{
	munmap((void *)c->co_stack, c->co_stack_sz);
}

#define CO_INCLUDE_FILE "co.c.inc"
#include "co.inc"
