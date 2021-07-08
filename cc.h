#ifndef CO_CONVENIENT_H
#define CO_CONVENIENT_H

#include <assert.h>

#include "co.h"

/**
 * Use co_*_fast() kind of routines.
 */
#ifndef CC_USE_FAST
# define CC_USE_FAST 0
#endif

#ifndef CC_EXPLICIT_SELF
# define CC_EXPLICIT_SELF 1
#endif

#ifndef CC_DEFAULT_STACK_SIZE
# define CC_DEFAULT_STACK_SIZE (1 << 15)
#endif

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

#if CC_EXPLICIT_SELF
# define CC_SELF_A_ self,
# define CC_SELF_P_ Coroutine *self,
# define CC_DEFINE_SELF_
#else
# define CC_SELF_A_
# define CC_SELF_P_
# define CC_DEFINE_SELF_ Coroutine *self = cc_self;
extern _Thread_local Coroutine *cc_self;
#endif

#define CC_DECLARE_MAIN(c) \
	char c##_cc_stack_[CO_STACK_SIZE_MIN]; \
	Coroutine c; \
	c.stack = c##_cc_stack_; \
	c.stack_size = sizeof c##_cc_stack_; \
	cc_reset_(&c); \
	CO_IF_(CC_EXPLICIT_SELF, ,cc_self = &c;)

int cc_create(Coroutine *c, void *routine(Coroutine *self, void *arg));

__attribute__((always_inline))
static inline void
cc_split_stack(size_t self_needed_size, Coroutine *c, size_t c_stack_size)
{
#ifdef __x86_64__
	register void *rsp asm("rsp");
	c->stack = (rsp + self_needed_size);
	c->stack_size = c_stack_size;
#endif
}

int cc_alloc_stack(Coroutine *c, size_t size);
int cc_free_stack(Coroutine *c);

__attribute__((CO_IF_(CC_USE_FAST, always_inline,)))
static CO_IF_(CC_USE_FAST, inline,) void *
cc_switch(Coroutine *from, Coroutine *to, void *arg)
{
	assert(from);
	assert(to);
	assert(from != to);
	assert(!to->done);
#if !CC_EXPLICIT_SELF
	cc_self = to;
#endif
	return CO_IF_(CC_USE_FAST, co_switch_fast, co_switch)(&from->sp, &to->sp, arg);
}

__attribute__((unused, CO_IF_(CC_USE_FAST, always_inline,)))
static CO_IF_(CC_USE_FAST, inline,) void *
cc_resume(CC_SELF_P_ Coroutine *c, void *arg)
{
	CC_DEFINE_SELF_;
	c->caller = self;
	return cc_switch(self, c, arg);
}

__attribute__((unused, CO_IF_(CC_USE_FAST, always_inline,)))
static CO_IF_(CC_USE_FAST, inline,) void *
cc_yield(CC_SELF_P_ void *arg)
{
	CC_DEFINE_SELF_;
	return cc_switch(self, self->caller, arg);
}

__attribute__((noreturn, unused, CO_IF_(CC_USE_FAST, always_inline,)))
static CO_IF_(CC_USE_FAST, inline,) void
cc_return(CC_SELF_P_ void *arg)
{
	CC_DEFINE_SELF_;
	self->done = 1;
	(void)cc_yield(CC_SELF_A_ arg);
	assert(!"Done coroutine resumed");
	__builtin_unreachable();
}

__attribute__((unused, CO_IF_(CC_USE_FAST, always_inline,)))
static CO_IF_(CC_USE_FAST, inline,) void *
cc_cancel(CC_SELF_P_ Coroutine *other, void *arg)
{
	other->cancelled = 1;
	return cc_resume(CC_SELF_A_ other, arg);
}

/**
 * Move coroutine stack to the given memory area.
 *
 * If stack contains self-referces, it shall not be waked up at its new
 * location.
 */
size_t cc_migrate(Coroutine *c, void *new_stack, size_t new_stack_size);

/* Private. */
void cc_reset_(Coroutine *self);

#endif /* CO_CONVENIENT_H */
