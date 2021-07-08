#ifndef CC_H
#define CC_H

#include <assert.h>

#include "co.h"

/**
 * Use co_*_fast() kind of routines.
 */
#ifndef CC_USE_FAST
# define CC_USE_FAST 0
#endif

#ifndef CC_DEFAULT_STACK_SIZE
# define CC_DEFAULT_STACK_SIZE (1 << 15)
#endif

#define CC_DECLARE_MAIN(c) \
	char c##_cc_stack_[CO_STACK_SIZE_MIN]; \
	Coroutine c; \
	c.stack = c##_cc_stack_; \
	c.stack_size = sizeof c##_cc_stack_; \
	cc_reset_(&c);

typedef struct Coroutine Coroutine;
struct Coroutine {
	void *sp;
	Coroutine *caller;
	unsigned char done;
	unsigned char cancelled;
#ifdef CC_COROUTINE_EXTRA
	CC_COROUTINE_EXTRA
#endif
	void *stack;
	size_t stack_size;
};

int cc_create(Coroutine *c, void *routine(Coroutine *self, void *arg));
int cc_alloc_stack(Coroutine *c, size_t size);
int cc_free_stack(Coroutine *c);

static void *
cc_switch(Coroutine *from, Coroutine *to, void *arg)
{
	assert(from);
	assert(to);
	assert(from != to);
	assert(!to->done);
	return
#if CC_USE_FAST
		co_switch_fast
#else
		co_switch
#endif
			(&from->sp, &to->sp, arg);
}

__attribute__((unused))
static void *
cc_resume(Coroutine *self, Coroutine *c, void *arg)
{
	c->caller = self;
	return cc_switch(self, c, arg);
}

__attribute__((unused))
static void *
cc_yield(Coroutine *self, void *arg)
{
	return cc_switch(self, self->caller, arg);
}

__attribute__((noreturn, unused))
static void
cc_return(Coroutine *self, void *arg)
{
	self->done = 1;
	(void)cc_yield(self, arg);
	assert(!"Done coroutine resumed");
	__builtin_unreachable();
}

__attribute__((unused))
static void *
cc_cancel(Coroutine *self, Coroutine *other, void *arg)
{
	other->cancelled = 1;
	return cc_resume(self, other, arg);
}

size_t cc_migrate(Coroutine *c, void *new_stack, size_t new_stack_size);

/* Private. */
void cc_reset_(Coroutine *self);

#endif /* CC_H */
