#if !(defined(__x86_64__))
# error "Unimplemented target architecture"
#endif

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

#include "co.h"

#define CO_ORDER_PUSH(x, y) x y
#define CO_ORDER_POP(x, y) y x

/* Callee saved registers. */
#ifdef __x86_64__
# define CO_REGS(order) \
	order(xmacro(r12), \
	order(xmacro(r13), \
	order(xmacro(r14), \
	order(xmacro(r15), \
	order(xmacro(rdx), \
	order(xmacro(rcx), \
	order(xmacro(rbx), \
	      xmacro(rbp))))))))
#endif

static co_t executor;

static __attribute__((naked))
void
co_trampoline(void)
{
	__asm__(
			/* Return value constructed from co_resume() for
			 * co_yield() must be transformed to the first
			 * argument of the routine at the very first call. */
			"mov %%rax,%%rdi\n\t"
			"ret\n\t"
		::
	);
	__builtin_unreachable();
}

static void
co_return(void);

void
co_init(co_t *c, void *routine(void *))
{
	c->done = 0;

	void *their_stack = c->stack;
	/* assert(!((uintptr_t)their_stack % 16)); */
	void *saved_stack;
#ifdef __x86_64__
	__asm__(
			"mov %%rsp,%[saved_stack]\n\t"

			/* Setup their stack and initial frame. */
			"mov %[their_stack],%%rsp\n\t"
#  if 1 <= CO_VERBOSE
			/* It should be really the address of last co_resume()
			 * if we would really would like to show something. */
			"lea 0(%%rip),%%rax\n\t" "push %%rax\n\t"
			"push (%%rbp)\n\t"
# endif
			"lea %[co_return],%%rax\n\t" "push %%rax\n\t"
			"push %[routine]\n\t"
			"lea %[co_trampoline],%%rax\n\t" "push %%rax\n\t"

			/* co_resume will restore garbage but it does not mean
			 * any security breaches. */
			"sub %[nsaved],%%rsp\n\t"
			"mov %%rsp,%[their_stack]\n\t"

			"mov %[saved_stack],%%rsp\n\t"

			: [saved_stack] "=r"(saved_stack),
			  [their_stack] "=&r"(their_stack)

			: [routine] "m"(routine),
			  [co_trampoline] "m"(co_trampoline),
			  [co_return] "m"(co_return),
			  [nsaved] "i"(
#define xmacro(name) +sizeof(uint64_t)
				CO_REGS(CO_ORDER_PUSH)
#undef xmacro
				/*+ sizeof(uint64_t) */ /* => !((%rsp + 8) % 16) -> MUST be popped accordingly */
			  )
	);
#endif
	c->frame = their_stack;
}

int
co_alloc_stack(co_t *c, size_t stack_size)
{
	void *p = mmap(NULL, stack_size, PROT_READ|PROT_WRITE,
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

	c->stack = p;
	c->stack_size = stack_size;

	return 0;
}

void
co_free_stack(co_t *c)
{
	munmap((void *)c->stack, c->stack_size);
}

#if 0
open_stream()
{

	co_yield(999); -> co_resume(&co_self->caller, 999);
	co_resume(&executor, NULL);
}
#endif

#if 0
__attribute__((naked))
void *
co_resume_nested(co_nested_t *c, void *arg)
{
}
#endif

__attribute__((naked))
void *
co_resume(co_t *c, void *arg)
{
#if 1
	fprintf(stderr, "co: from [%s] resume [%s] with %p\n", co_get_name(co_self), co_get_name(c), arg);
#endif

#ifdef __x86_64__
	__asm__(
#if 2 <= CO_VERBOSE && 0
			/* Get return address. Push it back immediately so we
			 * can ignore direction of the stack. */
			"pop %%rax\n\t"
			"push %%rax\n\t"
#endif
			/* Save our registers. */
#define xmacro(name) "push %%"#name"\n\t"
			CO_REGS(CO_ORDER_PUSH)
#undef xmacro
			"mov %%rsp,%[our_frame]\n\t"

			/* Update references. */
			"mov %[in_self],%[caller]\n\t"
			"mov %[c],%[out_self]\n\t"

#if 2 <= CO_VERBOSE && 0
			/* Update return address. */
			"mov %[stack],%%rsp\n\t"
			"push %%rax\n\t"
#endif
			/* Move to their frame. */
			"mov %[frame],%%rsp\n\t"

			/* Return passed value. */
			"mov %[arg],%%rax\n\t"

			/**
			 * IMPORTANT: It is not safe (makes GCC confused) to
			 * put instructions below pops unless callee saved
			 * registers are among clobbers.
			 */

			/* Restore their registers. */
#define xmacro(name) "pop %%"#name"\n\t"
			CO_REGS(CO_ORDER_POP)
#undef xmacro

			"ret\n\t"

			: [our_frame] "=m"(co_self->frame),
			  [out_self] "=m"(co_self),
			  [caller] "=m"(c->caller)

			: [frame] "r"(c->frame),
			  [stack] "r"(c->stack),
			  [in_self] "r"(co_self),
			  [c] "r"(c),
			  [arg] "r"(arg)

			: "rax");
#endif
	__builtin_unreachable();
}

static void __attribute__((naked))
co_return(void)
{
	co_self->done = 1;

	register void *ret asm("rax");
#if 1
	fprintf(stderr, "co: [%s] finished\n", co_get_name(co_self));
#endif
	(void)co_yield(ret);
	assert(!"Resumed terminated coroutine");
	__builtin_unreachable();
}

co_coroutine
void *coproc(void *arg)
{
	__asm__("int3");
	/* assert(!(((uintptr_t)rsp + 8) % 16)); */
	printf("deadbeef=%p\n", arg);
	printf("self=%p\n", co_self);
	printf("got=%p\n", co_yield((void *)0xbabe));
	return (void *)0x53536;
}

char stack[2048]/* __attribute((aligned(8)))*/;

static co_t *ptest;
int ma(int z)
{
	(void)z;
	co_self = &executor;

	co_set_name(&executor, "executor");

	memset(stack, 0xcd, sizeof stack);

	co_t test;
	ptest = &test;
	test.stack = (&stack)[1];
	test.stack_size = sizeof stack;
	co_init(&test, coproc/*, (void *)0xbeef*/);
	co_set_name(&test, "test");

	{
		register void *rsp asm("rbp");
		printf(">>> MAIN rsp=%p\n", rsp);
	}

	co_resume(&test, (void *)0xdeadbeef);

	{
		register void *rsp asm("rbp");
		printf(">>> MAIN rsp=%p\n", rsp);
	}

	co_resume(&test, (void *)0x777);

	return 0;
}

int main()
{

	return ma(9);
}

