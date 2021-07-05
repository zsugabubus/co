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
# define CO_STACK_DIRECTION -
#else
# error "Unimplemented target architecture"
#endif

typedef struct coroutine Coroutine;
struct coroutine {
	/**
	 * Frame pointer (%rsp).
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

#if 0
typedef struct co_stackless_t co_stackless_t;
struct co_stackless_t {
	Coroutine *co_caller;
	void *co_ip;
};

/* https://gcc.gnu.org/onlinedocs/gcc-4.7.2/gcc/Function-Attributes.html */
/* Attribute makes sure all registers are dead. */
/* __attribute__((returns_twice)) */
__attribute__((always_inline))
static inline co_stackless_t *
co_goto(co_stackless_t *c)
{
	__asm__ volatile(
			"int3\n"
			"lea .Lresume%=(%%rip),%%rdx\n\t"
			"mov %[saved],%%rax\n\t"
			"mov %%rdx,%[saved]\n\t"
			"jmp *%%rax\n\t"
			".Lresume%=:\n\t"
			: [saved] "+m"(c->co_ip)
			:
			: "cc", "rax", "rdx", "memory"
	);
	return c;
}
#endif

#endif /* CO_H */
