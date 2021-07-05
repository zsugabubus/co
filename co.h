#ifndef CO_H
#define CO_H

#include <string.h>

#define CO_ORDER_PUSH(x, y) x y
#define CO_ORDER_POP(x, y) y x

#ifdef __x86_64__
/* Callee-saved registers except base pointer. */
# define CO_CLOBBERED(order) \
	order(xmacro(r12), \
	order(xmacro(r13), \
	order(xmacro(r14), \
	order(xmacro(r15), \
	order(xmacro(rdx), \
	order(xmacro(rcx), \
	      xmacro(rbx)))))))
# define CO_STACK_SIZE(bottom /* Coroutine.co_stack */, top /* Coroutine.co_frame */) ((bottom) - (top))
#else
# error "Unimplemented target architecture"
#endif

typedef struct coroutine Coroutine;
struct coroutine {
	/**
	 * Frame pointer (%sp).
	 *
	 * User data for a coroutine can be stored aside Coroutine
	 * (struct { Coroutine c, ... }) or at the bottom of the stack. Before
	 * calling co_init you must adjust co_frame accordingly.
	 */
	void *co_frame;
	/**
	 * Coroutine that co_resume() was called from.
	 */
	Coroutine *co_caller;
	/**
	 * 1 if coroutine has return'ed; 0-ed by co_init().
	 *
	 * Calling a done coroutine results in undefined behavior.
	 */
	unsigned char co_done;
	void *co_stack; /**< Bottom of the stack. (Push-Pop must be valid.) */
	size_t co_stack_sz;
#ifdef CO_HAVE_VALGRIND
	unsigned long co_vg_stack_id;
#endif
#ifndef CO_NDEBUG
	char co_name[16 /* Taken from pthread_set_name(). */];
	unsigned char co_fast;
#endif
};

#define CO_INCLUDE_FILE "co.h.inc"
#include "co.inc"

int co_alloc_stack(Coroutine *c, size_t stack_size);

/**
 * Free resources acquired by co_alloc_stack().
 */
void co_free_stack(Coroutine *c);

static inline void
co_set_name(Coroutine *c, char const *name)
{
#ifndef CO_NDEBUG
	size_t n = strlen(name) + 1 /* NUL */;
	if (sizeof c->co_name < n)
		n = sizeof c->co_name;
	memcpy(c->co_name, name, n);
#else
	(void)c;
	(void)name;
#endif
}

__attribute__((always_inline))
static inline char const *
co_get_name(Coroutine const *c)
{
#ifndef CO_NDEBUG
	return c->co_name;
#else
	(void)c;
	return NULL;
#endif
}

/**
 * Resume stackless coroutine from state. Zero can be used for ground state.
 */
#define co_resumep(state) do { \
	goto *(&&co_start_ + state); \
co_start_: \
} while (0)

#define CO_TOKENPASTE__(a, b) a##b
#define CO_TOKENPASTE_(a, b) CO_TOKENPASTE__(a, b)

#ifndef CO_NDEBUG
# define CO_YIELDP_CHECK_(state) assert(((state) == &&CO_TOKENPASTE_(co_resume_at_, __LINE__) - &&co_start_))
#else
# define CO_YIELDP_CHECK_(state)
#endif

/**
 * Save coroutine state and yield with value.
 *
 * Note that co_yieldp() can be used to call nested stackless coroutines.
 */
#define co_yieldp(state, ... /* value */) do { \
	(state) = &&CO_TOKENPASTE_(co_resume_at_, __LINE__) - &&co_start_; \
	CO_YIELDP_CHECK_(state); \
	return __VA_ARGS__; \
CO_TOKENPASTE_(co_resume_at_, __LINE__): \
} while (0)

#endif /* CO_H */
