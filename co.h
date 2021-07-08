#ifndef CO_H
#define CO_H

/**
 * Symbols ending in _ are considered private.
 */

/**
 * CO_HAVE_FRAMEPOINTER := -fno-omit-frame-pointer present.
 *
 * %bp requires special handling when compiler treats %bp special and not as a
 * general purpose register. This behavior can also be changed on per function
 * basis using __attribute__((optimize("-f[no-]omit-frame-pointer"))).
 *
 * Misconfiguring this option either results in a compile-time error or runtime
 * overhead but never generates faulty code.
 */
#ifndef CO_HAVE_FRAMEPOINTER
# if defined(__OPTIMIZE__) && __OPTIMIZE__
#  define CO_HAVE_FRAMEPOINTER 0
# else
#  define CO_HAVE_FRAMEPOINTER 1
# endif
#endif

/**
 * CO_HAVE_REDZONE := -mred-zone present.
 *
 * -mred-zone := 1, CO_HAVE_REDZONE := 0, co_switch_fast() => bad things may
 * happen.
 */
#ifndef CO_HAVE_REDZONE
# define CO_HAVE_REDZONE 1
#endif

#define CO_IF_(cond, then, else) CO_IF__(cond, then, else)
#define CO_IF__(cond, then, else) CO_IF_##cond(then, else)
#define CO_IF_0(then, else) else
#define CO_IF_1(then, else) then

#define CO_TOKENPASTE__(x, y) x##y
#define CO_TOKENPASTE_(x, y) CO_TOKENPASTE__(x, y)

/**
 * Create a suspended coroutine. Routine will be called at the very first time
 * when coroutine is waked up.
 *
 * @param spp Location of the stack pointer, i.e. the state. Must be
 *            initialized to the required bottom of the stack.
 * @param return_routine Routine to call when control returns from passed
 *                       routine. Pass NULL if you do not want to return.
 */
#define co_create(spp, routine, return_routine) co_create_(spp, routine, return_routine, 0)
#define co_create_fast(spp, routine, return_routine) co_create_(spp, routine, return_routine, 1)
void co_create_(void **spp, void *routine(void **spp, void *arg), void return_routine(void **spp, void *return_value), unsigned fast);

/**
 * Transfer control between coroutines: suspend from and wake up to. Passed
 * arg will be returned by co_switch*() that made the suspension.
 *
 * Stack pointer of the suspended coroutine (from) is updated. When coroutine
 * is in a suspended state its stack and its stack pointer (*from) may be
 * freely moved unless it remains consistent. Use CO_STACK_SIZE() to determine
 * direction and size of the stack.
 *
 * Non-/co_*_fast() kind of routines have different undelying mechanism for
 * state preservation, thus wake up and suspension of a coroutine must be
 * performed by routines from the same kind.
 */
void *co_switch(void **restrict pfrom, void **restrict pto, void *arg);

#ifdef __x86_64__
# define CO_STACK_SIZE_MIN (128 /* Red zone. */ + 1 * 8 /* %bp */ + 6 * 8 /* Boilerplate. */)
# define CO_STACK_SIZE(bottom, top) ((bottom) - (top))
# define co_switch_fast(pfrom, pto, arg) ({ \
	void **pfrom_ = (pfrom); \
	void **pto_ = (pto); \
	void *arg_ = (arg); \
	__asm__ volatile( \
			/* Step over red zone. */ \
			CO_IF_(CO_HAVE_REDZONE, "sub $128,%%rsp\n\t", "") \
			CO_IF_(CO_HAVE_FRAMEPOINTER, "push %%rbp\n\t", "") \
 \
			/* Save our return address. */ \
			"lea .Lco_fast_resume%=(%%rip),%%r11\n\t" \
			"push %%r11\n\t" \
 \
			 /* Save our %sp. */ \
			"mov %%rsp,(%[our_frame])\n\t" \
 \
			/* Restore their %sp. */ \
			"mov %[frame],%%rsp\n\t" \
 \
			/* Resume they; basically a "ret" but uses branch predictor. */ \
			"pop %%r11\n\t" \
			CO_IF_(CO_HAVE_FRAMEPOINTER, "pop %%rbp\n\t", "") \
			CO_IF_(CO_HAVE_REDZONE, "add $128,%%rsp\n\t", "") \
			"jmp *%%r11\n\t" \
 \
			/* Will arrive here. */ \
			".align 8\n\t" \
		".Lco_fast_resume%=:\n\t" \
 \
			: /* We have to ensure that this address will not be \
			     %sp relative and GCC detects it as an output \
			     memory address. */ \
			  [our_frame] "+&r"(pfrom_) \
 \
			: [frame] "r"(*pto_), \
			   "a"(arg_) \
 \
			: "r11" \
	); \
	/* "jmp" was basically a function call, compiler must act accordingly. */ \
	__asm__(""::: \
			"memory", \
			"rax", "rbx", "rcx", "rdx", \
			CO_IF_(CO_HAVE_FRAMEPOINTER, "cc" /* Placeholder. */, "rbp" /* Can be clobbered thus "saved" by compiler. */), \
			"rsi", "rdi", \
			"r8", "r9", "r10", "r11", \
			"r12", "r13", "r14", "r15", \
			"cc"); \
	register void *rax asm("rax"); \
	rax; \
})
#endif

/**
 * Resume stackless coroutine from state. Zero can be used for ground state.
 */
#define co_resumep(pstate) do { \
	goto *(&&co_start_ + *(pstate)); \
co_start_:; \
} while (0)

/**
 * Save coroutine state and yield with value.
 *
 * Note that co_yieldp() can be used to call nested stackless coroutines.
 */
#define co_yieldp(pstate, ... /* value */) do { \
	void *state = (void *)(*(pstate) = &&CO_TOKENPASTE_(co_resume_at_, __LINE__) - &&co_start_); \
	(void)state; \
	assert(state == &&CO_TOKENPASTE_(co_resume_at_, __LINE__) - &&co_start_); \
	return __VA_ARGS__; \
CO_TOKENPASTE_(co_resume_at_, __LINE__):; \
} while (0)

#endif /* CO_H */
