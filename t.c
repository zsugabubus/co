#include "co.c"

static co_t executor;

void *coproc(void *arg)
{
	/* register void *rsp asm("rsp"); */
	/* printf("rrrsp %p\n", rsp); */

	/* assert(!(((uintptr_t)rsp + 8) % 16)); */

	printf("deadbeef=%p\n", arg);
	printf("self=%p\n", co_self);
	for (int i = 0; i < 20000000; ++i) {
		/* printf("%d\n", i); */
		co_yield(&i);
	}
	printf("got=%p\n", co_yield((void *)0xbabe));
	return (void *)0x53536;
}

char stack[40096]/* __attribute((aligned(8)))*/;

int ma(int z, int k, int b, int t, int u, void *c)
{
	(void)z, (void)k, (void)b, (void)t, (void)u, (void)c;
	co_self = &executor;

	/* void *stack = alloca(10000); */
	/* Dummy stack needed for debug info. */
	char dummy[16];
	co_self->co_stack = dummy;

	co_set_name(&executor, "executor");

	/* memset(stack, 0xcd, sizeof stack); */

	co_t test;
	test.co_stack = (&stack)[1];
	test.co_stack_sz = sizeof stack;

	printf("k\n");

	co_alloc_stack(&test, 1 << 16);

	printf("bb\n");

	co_init(&test, coproc/*, (void *)0xbeef*/);

	co_set_name(&test, "test");

	printf("a\n");
	{
		/* register void *rsp asm("rbp");
		printf(">>> MAIN y rsp=%p\n", rsp); */
	}

	co_resume(&test, (void *)0xdeadbeef);

	{
		/* register void *rsp asm("rbp");
		printf(">>> MAIN x rsp=%p\n", rsp); */
	}

	while (!test.co_done)
		co_resume(&test, (void *)0x777);

	return 0;
}

int z()
{
	return ma(9, 4, 1, 3, 2, &z);
}

void
sl(co_stackless_t *c)
{
	c = co_goto(c);
	c = co_goto(c);
}

int main()
{
	/* co_stackless_t c;
	c.ip = sl;
	co_goto(&c);
 */
	return z();
}
