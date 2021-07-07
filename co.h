#ifndef CO_H
#define CO_H

/**
 * -fno-omit-frame-pointer is present.
 */
#ifndef CO_HAVE_BP
/* At worst case it is saved but not used. */
/* !CO_HAVE_BP <=> asm("":::"rbp") compiles. */
# define CO_HAVE_BP 0
#endif

#define CO_IF_(cond, then, else) CO_IF__(cond, then, else)
#define CO_IF__(cond, then, else) CO_IF_##cond(then, else)
#define CO_IF_0(then, else) else
#define CO_IF_1(then, else) then

#define CO_TOKENPASTE__(x, y) x##y
#define CO_TOKENPASTE_(x, y) CO_TOKENPASTE__(x, y)

#define co_fast_coroutine CO_IF_(CO_HAVE_BP, , __attribute__((optimize("-fomit-frame-pointer"))))

/**
 * Initialize stack for coroutine. Coroutine will be waked up on first
 * co_switch() and will routine() will receive that arg.
 *
 * @param fast Must be 1 or 0. (May will be flags later.)
 */
void co_create(void **pstack, void *routine(void **pstack, void *arg), unsigned fast);

/**
 * Perform context switching.
 *
 * Save current context into from and load new context from to. Passed arg will
 * be returned for the woken up co_switch().
 */
void *co_switch(void **restrict pfrom, void **restrict pto, void *arg);

#ifdef __x86_64__
# define CO_STACK_SIZE_MIN(fast) (((fast ? 1 : 8) /* General registers. */ + (1 + 1) /* IP. */) * 8)
# define CO_STACK_SIZE(bottom, top) ((bottom) - (top))
# define co_switch_fast(pfrom, pto, arg) ({ \
	void **pfrom_ = (pfrom); \
	void **pto_ = (pto); \
	void *arg_ = (arg); \
	__asm__ volatile( \
			CO_IF_(CO_HAVE_BP, "push %%rbp\n\t", "") \
 \
			/* Save our return address. */ \
			"lea .Lco_fast_resume%=(%%rip),%%r11\n\t" \
			"push %%r11\n\t" \
 \
			 /* Save our %sp. */ \
			"mov %%rsp,%[our_frame]\n\t" \
 \
			/* Restore their %sp. */ \
			"mov %[frame],%%rsp\n\t" \
 \
			/* Resume they; basically a "ret" but uses branch predictor. */ \
			"pop %%r11\n\t" \
			"jmp *%%r11\n\t" \
 \
			/* Will arrive here. */ \
			".align 8\n\t" \
		".Lco_fast_resume%=:\n\t" \
 \
			CO_IF_(CO_HAVE_BP, "pop %%rbp\n\t", "") \
 \
			: [our_frame] "=m"(*pfrom_) \
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
			CO_IF_(CO_HAVE_BP, "cc" /* Placeholder. */, "rbp"), "rsi", "rdi", \
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
#define co_resumep(state) do { \
	goto *(&&co_start_ + state); \
co_start_: \
} while (0)

/**
 * Save coroutine state and yield with value.
 *
 * Note that co_yieldp() can be used to call nested stackless coroutines.
 */
#define co_yieldp(state, ... /* value */) do { \
	(state) = &&CO_TOKENPASTE_(co_resume_at_, __LINE__) - &&co_start_; \
	assert(((state) == &&CO_TOKENPASTE_(co_resume_at_, __LINE__) - &&co_start_)); \
	return __VA_ARGS__; \
CO_TOKENPASTE_(co_resume_at_, __LINE__): \
} while (0)

#endif /* CO_H */
