#include <stdio.h>
#include <assert.h>

#include "co.h"

static Coroutine executor;

void *coproc(Coroutine *me, void *arg)
{
	/* register void *rsp asm("rsp"); */
	/* printf("rrrsp %p\n", rsp); */

	/* assert(!(((uintptr_t)rsp + 8) % 16)); */

	printf("deadbeef=%p\n", arg);
	printf("self=%p\n", co_self);

	assert((void *)co_self == me);
	assert((void *)&executor == me->co_caller);

	for (int i = 0; i < 20000000; ++i) {
		/* printf("i=%d\n", i); */
		co_switch_fast(me, &executor, &i);
	}
	printf("got=%p\n", co_yield_fast((void *)0xbabe));
	return (void *)0x53536;
}

char stack[40096]/* __attribute((aligned(8)))*/;

int ma(int z, int b, int t, int u, void *c)
{
	(void)z, (void)b, (void)t, (void)u, (void)c;
	co_self = &executor;

	/* void *stack = alloca(10000); */
	/* Dummy stack needed for debug info. */
	char dummy[16];
	co_self->co_stack = dummy;

	co_set_name(&executor, "executor");

	/* memset(stack, 0xcd, sizeof stack); */

	Coroutine test;
	test.co_stack = (&stack)[1];
	test.co_stack_sz = sizeof stack;

	printf("k\n");

	co_alloc_stack((void *)&test, 1 << 16);

	printf("bb\n");

	co_create_fast(&test, coproc);
	test.co_caller = &executor;

	co_set_name((void *)&test, "test");

	co_resume_fast(&test, (void *)0xdeadbeef);

	while (!test.co_done) {
		/* printf("bicikil\n"); */
		co_resume_fast(&test, (void *)0x777);
	}

	return 0;
}

int z()
{
	return ma(9, 1, 3, 2, &z);
}

#if 0
void
sl(co_stackless_t *c)
{
	c = co_goto(c);
	c = co_goto(c);
}
#endif

int main()
{
	/* co_stackless_t c;
	c.ip = sl;
	co_goto(&c);
 */
	return z();
}
