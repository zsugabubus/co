#if !(defined(__x86_64__))
# error "Unimplemented target architecture"
#endif

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

#define CO_ORDER_PUSH(x, y) x y
#define CO_ORDER_POP(x, y) y x

/* Callee-saved registers. */
#ifdef __x86_64__
# define CO_REGS(order) \
	order(xmacro(r12), \
	order(xmacro(r13), \
	order(xmacro(r14), \
	order(xmacro(r15), \
	order(xmacro(rdx), \
	order(xmacro(rcx), \
	      xmacro(rbx)))))))
/* Except %rbp. */
#endif

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
#if CO_HAVE_VALGRIND
	c->vg_stack_id = VALGRIND_STACK_REGISTER(c->stack, c->stack_sz);
#endif

	c->co_done = 0;

	void *their_stack = c->co_stack;
	void *saved_stack = saved_stack;

#ifdef __x86_64__
	__asm__(
			"mov %%rsp,%[saved_stack]\n\t"

			/* Setup their stack and initial frame. */
			"mov %[their_stack],%%rsp\n\t"
# if 2 <= CO_VERBOSE
			/* It should be really the address of last co_resume()
			 * if we would really would like to show something. */
			/* "lea 0(%%rip),%%rax\n\t" "push %%rax\n\t" */
			"sub $0x10,%%rsp\n\t"
			/* return address */
			/* previous %rbp value */
# endif
			"lea %[co_return],%%rax\n\t" "push %%rax\n\t"
			"push %[routine]\n\t"
			"lea %[co_trampoline],%%rax\n\t" "push %%rax\n\t"

# if 2 <= CO_VERBOSE
			/* Set rbp to reference our dummy frame above. */
			"lea -0x10(%[their_stack]),%%rax\n\t" "push %%rax\n\t"
# endif

			/* Registers will receive garbage. */
			"sub %[nsaved],%%rsp\n\t"

			"mov %%rsp,%[their_stack]\n\t"

			"mov %[saved_stack],%%rsp\n\t"

			: [saved_stack] "+r"(saved_stack),
			  [their_stack] "+r"(their_stack)

			: [routine] "r"(routine),
			  [co_trampoline] "m"(co_trampoline),
			  [co_return] "m"(co_return),
			  [nsaved] "i"(
#define xmacro(name) +sizeof(uint64_t)
				CO_REGS(CO_ORDER_PUSH)
#undef xmacro
# if !(2 <= CO_VERBOSE)
				+sizeof(uint64_t)
# endif
				/*+ sizeof(uint64_t) */ /* => !((%rsp + 8) % 16) -> MUST be popped accordingly */
			  )
			: "rax"
	);
#endif
	c->co_frame = their_stack;
}

int
co_alloc_stack(co_t *c, size_t stack_sz)
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

	return 0;
}

void
co_free_stack(co_t *c)
{
	munmap((void *)c->co_stack, c->co_stack_sz);
}

__attribute__((naked, noinline))
void *
co_resume(co_t *c, void *arg)
{
#if 0
	fprintf(stderr, "co: from [%s] resume [%s] (st=%p) with %p\n", co_get_name(co_self), co_get_name(c), c->frame, arg);
#endif

#ifdef __x86_64__
	__asm__(
#if 2 <= CO_VERBOSE
			/* Get return address. Push it back immediately so we
			 * can ignore direction of the stack. */
			"pop %%rax\n\t"
			"push %%rax\n\t"
			/* "mov (%%rsp),%%rax\n\t" */
#endif
			/* Save our registers. */
			"push %%rbp\n\t"
#define xmacro(name) "push %%"#name"\n\t"
			CO_REGS(CO_ORDER_PUSH)
#undef xmacro

			"mov %%rsp,%[our_frame]\n\t"

			/* Update references. */
			"mov %[in_self],%[caller]\n\t"
			"mov %[c],%[out_self]\n\t"

#if 2 <= CO_VERBOSE
			/* Update return address and %rbp. */
			"mov %[stack],%%rsp\n\t"
			"push %%rax\n\t"
			"push %%rbp\n\t"
#endif
			/* Move to their frame. */
			"mov %[frame],%%rsp\n\t"

			/* Return passed value. */
			"mov %[arg],%%rax\n\t"

			/* Restore their registers. */
#define xmacro(name) "pop %%"#name"\n\t"
			CO_REGS(CO_ORDER_POP)
#undef xmacro
			"pop %%rbp\n\t"

			"ret\n\t"

			: [our_frame] "=m"(co_self->co_frame),
			  [out_self] "=m"(co_self),
			  [caller] "=m"(c->co_caller)

			: [frame] "r"(c->co_frame),
			  [stack] "r"(c->co_stack),
			  [in_self] "r"(co_self),
			  [c] "r"(c),
			  [arg] "r"(arg)

			:
#define xmacro(name) #name,
			  CO_REGS(CO_ORDER_POP)
#undef xmacro
			  /* Return value. */
			  "rax");
#endif
}

static void __attribute__((naked))
co_return(void)
{
	register void *rax asm("rax");
	void *arg = rax;

	co_self->co_done = 1;

#if 0
	fprintf(stderr, "co: [%s] finished\n", co_get_name(co_self));
#endif

	(void)co_yield(arg);
	assert(!"Resumed terminated coroutine");
	__builtin_unreachable();
}
