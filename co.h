#ifndef CO_H
#define CO_H

#include <string.h>

#include "config.h"

/**
 * Execution contexts
 * main                           fn(init_arg)
 *
 * if (co_alloc_stack(callee, 1 << 12) < 0)
 *     abort();
 * co_init(callee, fn)
 *
 * co_self := caller
 *      co_resume(callee, x)
 *      .                         co_self := callee
 *      .                         init_arg := x
 *      .
 * y :=                           co_yield(y)
 * ...
 *                                return z;
 * callee->done := 1
 */

typedef struct co_t co_t;
struct co_t {
	void *co_frame;
	co_t *co_caller;
	unsigned char co_done;
	void *co_stack; /* Bottom. */
	size_t co_stack_sz;
#ifdef CO_HAVE_VALGRIND
	unsigned long co_vg_stack_id;
#endif
#if 1 <= CO_VERBOSE
	char co_name[16];
#endif
};

typedef struct co_stackless_t co_stackless_t;
struct co_stackless_t {
	co_t *co_caller;
	void *co_ip;
};

#define co_coroutine __attribute__((naked))

/**
 * Currently executing coroutine.
 */
/* _Thread_local */ co_t *co_self;

int co_alloc_stack(co_t *c, size_t stack_size);

/**
 * Free resources acquired by co_alloc_stack().
 */
void co_free_stack(co_t *c);

/**
 * Initialize coroutine.
 */
void co_init(co_t *c, void *routine(void *arg));

static inline void
co_set_name(co_t *c, char const *name)
{
#if 1 <= CO_VERBOSE
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
co_get_name(co_t const *c)
{
#if 1 <= CO_VERBOSE
	return c->co_name;
#else
	(void)c;
	return NULL;
#endif
}

/**
 * Resume suspended coroutine c (that co_resume()'d another coroutine) and use
 * arg for return value.
 *
 * On first call after co_init(), coroutine will be woken up from its initial
 * suspended state and will receive arg as its first argument.
 *
 * Any suspended coroutine can be resumed from any other coroutine (including
 * co_self because it will be suspended by the time of context switch).
 */
void *co_resume(co_t *c, void *arg);

#if 1
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

/**
 * Resume caller coroutine.
 *
 * @see co_resume
 */
__attribute__((always_inline))
static inline void *
co_yield(void *arg)
{
#if 0
	printf("yield to %s\n", co_get_name(co_self->caller));
#endif
	return co_resume(co_self->co_caller, arg);
}

#endif /* CO_H */
