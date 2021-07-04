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
	void *frame;
	co_t *caller;
	unsigned char done;
	void *stack; /* Bottom. */
	size_t stack_size;
#if 1 <= CO_VERBOSE
	char name[16];
#endif
};

#define co_coroutine __attribute__((naked))

# if 0
co_stackless
void *hello(World *arg)
{
	int z = arg * arg;
}
#endif

typedef struct co_nested_t co_nested_t;
struct co_nested_t {
	co_t c;
};

/**
 * Currently executing coroutine.
 */
/* _Thread_local  */ co_t *co_self;

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
	if (sizeof c->name < n)
		n = sizeof c->name;
	memcpy(c->name, name, n);
#else
	(void)c;
	(void)name;
#endif
}

static inline char const *
co_get_name(co_t const *c)
{
#if 0 < CO_NAME_SIZE
	return c->name;
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

/**
 * Resume caller coroutine.
 *
 * @see co_resume
 */
static inline void *
co_yield(void *arg)
{
	return co_resume(co_self->caller, arg);
}

#endif /* CO_H */
