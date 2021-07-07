#include "co.h"

#define CO_ORDER_PUSH(x, y) x y
#define CO_ORDER_POP(x, y) y x

#ifdef __x86_64__
/* Except %bp. */
# define CO_CALLEE_SAVED(order) \
	order(xmacro(r12), \
	order(xmacro(r13), \
	order(xmacro(r14), \
	order(xmacro(r15), \
	order(xmacro(rbx), \
	order(xmacro(rcx), \
	      xmacro(rdx)))))))
#else
# error "Unimplemented target architecture"
#endif

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
			"pop %%rax\n\t"
			"jmp *%%rax\n\t"
		::
	);
#endif
	__builtin_unreachable();
}

void
co_create(void **pstack, void *routine(void **, void *), void return_routine(void **pstack, void *return_value), unsigned fast)
{
	void *their_stack = *pstack;
	void *saved_stack = saved_stack;

#ifdef __x86_64__
	__asm__(
			"mov %%rsp,%[saved_stack]\n\t"

			/* Setup their stack and required boilerplate frame. */
			"mov %[their_stack],%%rsp\n\t"

			"lea %[co_trampoline],%%rax\n\t"

			"push %[return_routine]\n\t"
			"test %[return_routine],%[return_routine]\n\t"
			"jz .L0\n\t"
			"push %[pstack]\n\t"
			"push %%rax\n\t"
		".L0:\n\t"

			"push %[routine]\n\t"
			"push %[pstack]\n\t"

			/* Registers will receive garbage. */
			"sub %[nsaved],%%rsp\n\t"

			"push %%rax\n\t"
			"mov %%rsp,%[their_stack]\n\t"

			"mov %[saved_stack],%%rsp\n\t"

			: [saved_stack] "+r"(saved_stack),
			  [their_stack] "+r"(their_stack)

			: [routine] "r"(routine),
			  [pstack] "r"(pstack),
			  [return_routine] "r"(return_routine),
			  [co_trampoline] "m"(co_trampoline),
			  [nsaved] "r"(
				(sizeof 0 - sizeof 0)
# define xmacro(name) +8
				+ (fast
				? 0
# if CO_RESTORE_BP
					xmacro(rbp)
# endif
				: 0
					xmacro(rbp)
					CO_CALLEE_SAVED(CO_ORDER_PUSH)
				)
# undef xmacro
			  )
			: "rax"
	);
#endif
	*pstack = their_stack;
}

__attribute__((optimize("-O3"), naked, hot))
void *
co_switch(void **restrict pfrom, void **restrict pto, void *arg)
{
#ifdef __x86_64__
	__asm__ volatile(
			/* Make return value to be pop'ed first on resume. */
			"pop %%r11\n\t"

			/* Save our registers. */
# define xmacro(name) "push %%"#name"\n\t"
			xmacro(rbp)
			CO_CALLEE_SAVED(CO_ORDER_PUSH)
# undef xmacro

			"push %%r11\n\t"
			"mov %%rsp,%[our_frame]\n\t"

			/* Move to their frame. */
			"mov %[frame],%%rsp\n\t"

			"pop %%r11\n\t"

			/* Restore their registers. */
# define xmacro(name) "pop %%"#name"\n\t"
			CO_CALLEE_SAVED(CO_ORDER_POP)
			xmacro(rbp)
# undef xmacro

			"jmp *%%r11\n\t"

			: [our_frame] "=m"(*pfrom)

			: [frame] "r"(*pto),
			   "a"(arg)

			:
# define xmacro(name) #name,
			  CO_CALLEE_SAVED(CO_ORDER_POP)
# undef xmacro
#  if !CO_RESTORE_BP
			  "rbp",
#  endif
			  "r11"
			);
#endif
}
