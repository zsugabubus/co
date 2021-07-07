#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "co.h"

#define GET_STACK_BOTTOM(stack) ((&stack)[CO_STACK_SIZE(0, 1) < 0])
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

#define ARG(x) ((void *)(uintptr_t)(x))

typedef struct Coroutine Coroutine;
struct Coroutine {
	void *frame;
	Coroutine *caller;
	int done;
};

typedef struct {
	uintptr_t state;
	int i;
	char buf[20];
} StacklessCoroutine;

static Coroutine executor;

static void
co_yield(Coroutine *me, void *arg)
{
	co_switch(&me->frame, &me->caller->frame, arg);
}

static void *
co_resume(Coroutine *me, Coroutine *you, void *arg)
{
	you->caller = me;
	return co_switch(&me->frame, &you->frame, arg);
}

int
f(Coroutine *me, int depth, int i)
{
	static char buf[50];

	sprintf(buf, "f(%d, %d)", depth, i);
	co_yield(me, buf);

	return i <= 1 ? 1 : f(me, depth + 1, i - 1) + f(me, depth + 1, i - 2);
}

void *
sf(void **pframe, void *arg)
{
	Coroutine *me = container_of(pframe, Coroutine, frame);

	printf("%s ", (char *)arg);
	co_switch_fast(&me->frame, &executor.frame, "world!");

	co_switch_fast(&me->frame, &me->frame, ARG(1));

	for (int i = 0; i < 20000000; ++i) {
		/* printf("%d\n", i); */
		co_switch_fast(&me->frame, &executor.frame, ARG(1));
	}

	printf("---\n");
	co_switch_fast(&me->frame, &executor.frame, ARG(0));
}

void *
fibonacci_routine(void **pframe, void *n)
{
	Coroutine *me = container_of(pframe, Coroutine, frame);

	co_switch(&me->frame, &executor.frame, "bye");
	void *res = ARG(f(me, 0, (uintptr_t)n));

	co_yield(me, "k");

	me->done = 1;
	co_yield(me, res);
}

static void
fibonacci(void)
{
	Coroutine *me = &executor;

	char callee_stack[1 << 12] __attribute__((aligned(16)));
	Coroutine c;
	c.done = 0;
	c.frame = GET_STACK_BOTTOM(callee_stack);

	printf("c.stack= -%p\n", c.frame);
	co_create(&c.frame, fibonacci_routine, 0);

	/* printf("me = %p\n", co_switch(&me->frame, &me->frame, ARG(20))); */
	/* __asm__("int3"); */
	co_resume(me, &c, (void *)(uintptr_t)4);

	void *buf = "create";

	for (;;) {
		printf("stack size is %zd bytes at %s @%p %p\n", CO_STACK_SIZE(GET_STACK_BOTTOM(callee_stack), (char *)c.frame), (char *)buf, c.frame, executor.frame);
		buf = co_resume(me, &c, NULL);
		if (c.done) {
			printf("result is %d %d %p %p\n", c.done, (int)(uintptr_t)buf, c.frame, executor.frame);
			break;
		}
	}
}

static int
sl(StacklessCoroutine *c)
{
	co_resumep(&c->state);

	while (c->i < 20000000)
		co_yieldp(&c->state, 1);

	return 0;
}

int main(int argc, char *argv[])
{
	(void)argc;

	char executor_stack[CO_STACK_SIZE_MIN(0)] __attribute__((aligned(16)));

	executor.frame = GET_STACK_BOTTOM(executor_stack);

	char which = 1 < argc ? argv[1][0] : '\0';
#define T(l, name) if ((!which || which == #l[0]) && (printf("=== [%c] %s ===\n", #l[0], #name), 1))

	T(l, stackless) {
		StacklessCoroutine c;
		c.state = 0;
		for (; sl(&c); ++c.i);
	}

	T(f, fibonacci)
		fibonacci();

#if 0
	T(s, self)
		for (int i = 0; i < 20000000 * 2;)
			++*(int *)co_switch(&executor.frame, &executor.frame, &i);

	T(S, self-fast)
		for (int i = 0; i < 20000000 * 2;)
			++*(int *)co_switch_fast(&executor.frame, &executor.frame, &i);
#endif

	T(2, switch2) {
		char spinner_stack[1 << 12] __attribute__((aligned(16)));
		Coroutine spinner;
		spinner.frame = GET_STACK_BOTTOM(spinner_stack);

		co_create(&spinner.frame, sf, 1);

		/* printf("yes = %s\n", (char *)co_switch_fast(&executor.frame, &executor.frame, "no")); */

		printf("%s\n", (char *)co_switch_fast(&executor.frame, &spinner.frame, "Hello"));
		while (co_switch_fast(&executor.frame, &spinner.frame, (void *)0x777));
	}
}
