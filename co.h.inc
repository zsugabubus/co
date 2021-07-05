#if !CO_FAST
/**
 * Currently executing coroutine. Saved by co_resume().
 */
extern _Thread_local Coroutine *co_self;
#endif

/**
 * Initialize coroutine. Coroutine is suspended before entering routine. First time switching
 *
 * Coroutine c placed is place
 *
 * On first call after co_init(), coroutine will be woken up from its initial
 * suspended state and will receive arg as its first argument.
 *
 */
void CO_MAKE_NAME(, co_create)(Coroutine *c, void *routine(Coroutine *c, void *arg));

/**
 * Perform context switching.
 *
 * Save current context into from and load new context from to. Passed arg will
 * be returned for the woken up co_switch.
 *
 * Any suspended coroutine can be resumed from any other coroutine (including
 * co_self because it will be suspended by the time of context switch).
 */
void *
CO_MAKE_NAME(co_internal_, co_switch)(Coroutine *from, Coroutine *to, void *arg);

#if CO_FAST
__attribute__((always_inline))
static inline void *
co_switch_fast(Coroutine *from, Coroutine *to, void *arg)
{
	/**
	 * For small configuration all registers are saved on stack internally.
	 *
	 * For a faster configuration we tell caller that we do not care
	 * about callee-saved registers and they will not be backed up so it
	 * must be done by caller if interested in their contents.
	 *
	 * Latter configuration will be at least as fast as former because
	 * caller can cherry pick which register it wants to be saved thus we
	 * can avoid push-pop'ing every calle-saved registers.
	 */
	__asm__("":::
#define xmacro(name) #name,
		  CO_CLOBBERED(CO_ORDER_PUSH)
#undef xmacro
		"memory");
	return co_internal_co_switch_fast(from, to, arg);
}
#endif

/**
 * Resume suspended coroutine c with arg and set its caller coroutine to
 * ourself.
 *
 * @see co_switch
 */
__attribute__((hot))
static inline void *
CO_MAKE_NAME(, co_resume)(Coroutine *c, void *arg)
{
	Coroutine *from = co_self;
	Coroutine *to = c;

	to->co_caller = from;
	co_self = (void *)to;

	return CO_MAKE_NAME(, co_switch)(from, to, arg);
}

/**
 * Resume caller coroutine but do not modify its caller; otherwise it behaves
 * just like co_resume().
 *
 * @see co_resume
 */
__attribute__((hot))
static inline void *
CO_MAKE_NAME(, co_yield)(void *arg)
{
#if 0
	printf("yield to %s\n", co_get_name(co_self->caller));
#endif
	Coroutine *from = co_self;
	Coroutine *to = from->co_caller;
	assert(to);

	co_self = (void *)to;

	return CO_MAKE_NAME(, co_switch)(from, to, arg);
}

/* vim: set ft=c: */
