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

 __attribute__((naked))
static void
co_trampoline(void)
{
#ifdef __x86_64__
	__asm__ volatile(
			"pop %%rdi\n\t" /* Pop => 1st arg. */
			"mov %%rax,%%rsi\n\t" /* Return value => 2nd arg. */
			"pop %%rax\n\t"
			"jmp *%%rax\n\t" /* Pop => Call. */
		::
	);
#endif
}

/* Setup stack for a suspended coroutine. */
void
co_create_(void **spp, void *routine(void **, void *), void return_routine(void **, void *), unsigned fast)
{
	register void *saved_sp = saved_sp;

#ifdef __x86_64__
	__asm__ volatile(
			"mov %%rsp,%[saved_sp]\n\t"

			"mov (%[spp]),%%rsp\n\t"

			"lea %[co_trampoline],%%rax\n\t"

			"push %[return_routine]\n\t"
			"test %[return_routine],%[return_routine]\n\t"
			"jz .L0%=\n\t"
			"push %[spp]\n\t"
			"push %%rax\n\t"
		".L0%=:\n\t"

			"push %[routine]\n\t"
			"push %[spp]\n\t"

			/* Registers will receive garbage. */
			"sub %[nsaved],%%rsp\n\t"

			"push %%rax\n\t"
			"mov %%rsp,(%[spp])\n\t"

			"mov %[saved_sp],%%rsp\n\t"

			: [saved_sp] "+&r"(saved_sp)

			: [routine] "r"(routine),
			  [spp] "r"(spp),
			  [return_routine] "r"(return_routine),
			  [co_trampoline] "m"(co_trampoline),
			  [nsaved] "g"(
				(sizeof 0 - sizeof 0)
# define xmacro(name) +8
				+ (fast
				? 0
# if CO_HAVE_FRAMEPOINTER
					xmacro(rbp)
# endif
					+ CO_IF_(CO_HAVE_REDZONE, 128, 0)
				: 0
					xmacro(rbp)
					CO_CALLEE_SAVED(CO_ORDER_PUSH)
				)
# undef xmacro
			  )
			: "rax", "memory"
	);
#endif
}

__attribute__((naked, hot))
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

			/* /\ */

			/* Move to their frame. */
			"mov %[frame],%%rsp\n\t"

			"pop %%r11\n\t"

			/* Restore their registers. */
# define xmacro(name) "pop %%"#name"\n\t"
			CO_CALLEE_SAVED(CO_ORDER_POP)
			xmacro(rbp)
# undef xmacro

			"jmp *%%r11\n\t"

			: [our_frame] "=&m"(*pfrom)

			: [frame] "r"(*pto),
			   "a"(arg)

			:
# define xmacro(name) #name,
			  CO_CALLEE_SAVED(CO_ORDER_POP)
# undef xmacro
#  if !CO_HAVE_FRAMEPOINTER
			  "rbp",
#  endif
			  "r11"
			);
#endif
}
