#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>

#include "cc.h"

#define CC_STACK_GROWSDOWN (CO_STACK_SIZE(0, 1) < 0)

static void
cc_returned(Coroutine *self, void *arg)
{
	(void)self;
	cc_return(CC_SELF_A_ arg);
}

void
cc_reset_(Coroutine *self)
{
	self->sp = self->stack + (CC_STACK_GROWSDOWN ? self->stack_size : 0);
	self->caller = NULL;
	self->done = 0;
	self->cancelled = 0;
}

int
cc_create(Coroutine *self, void *routine(Coroutine *self, void *arg))
{
	if (!self->stack) {
		int res =  cc_alloc_stack(self, self->stack_size ? self->stack_size : CC_DEFAULT_STACK_SIZE);
		if (res < 0)
			return res;
	}

	if (self->stack_size < CO_STACK_SIZE_MIN)
		return -ENOSPC;

	cc_reset_(self);
	CO_IF_(CC_USE_FAST, co_create_fast, co_create)(&self->sp, (void *)routine, (void *)cc_returned);

	return 0;
}

int
cc_alloc_stack(Coroutine *c, size_t stack_size)
{
	void *p = mmap(NULL, stack_size,
			PROT_READ | PROT_WRITE,
#ifdef MAP_STACK
			MAP_STACK |
#endif
#ifdef MAP_GROWSDOWN
			(CC_STACK_GROWSDOWN < 0 ? MAP_GROWSDOWN : 0) |
#endif
			MAP_ANONYMOUS |
			MAP_PRIVATE,
			-1, 0);
	if (MAP_FAILED == p)
		return -errno;

	c->stack = p;
	c->stack_size = stack_size;

	return 0;
}

int
cc_free_stack(Coroutine *c)
{
	if (munmap((char *)c->stack, c->stack_size) < 0)
		return -errno;

	return 0;
}

size_t
cc_migrate(Coroutine *c, void *new_stack, size_t new_stack_size)
{
	size_t used_size = CO_STACK_SIZE(c->stack + (CC_STACK_GROWSDOWN ? c->stack_size : 0), c->sp);
	if (!new_stack)
		return used_size;
	assert(used_size <= new_stack_size);

	if (CC_STACK_GROWSDOWN < 0)
		memcpy(new_stack + new_stack_size - used_size, c->sp, used_size);
	else
		memcpy(new_stack, c->stack, used_size);

	c->sp = (char *)c->sp - (char *)c->stack + (char *)new_stack;
	c->stack = new_stack;
	c->stack_size = new_stack_size;

	return used_size;
}
