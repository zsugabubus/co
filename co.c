#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#if !(defined(__x86_64__))
# error "Unimplemented target architecture"
#endif

_Static_assert(sizeof(long) == sizeof(size_t));

#define REVERSE(a, b) b, a

/* Stack restored different way. */
#define CALLEE_SAVED \
	xmacro(long, r12) \
	xmacro(long, r13) \
	xmacro(long, r14) \
	xmacro(long, r15) \
	xmacro(long, rdx) \
	xmacro(long, rcx) \
	xmacro(long, rbx) \
	xmacro(long, rbp)

#if 0
typedef struct {
#define xmacro(type, name) type name;
	CALLEE_SAVED
#undef xmacro
	long eip;
} co_ctx_t;
#endif

typedef struct co_t co_t;

struct co_t {
	char name[16];
	void *frame;
	void *stack;
	size_t stack_size;
	/* co_ctx_t ctx; */
	co_t *caller;
};

static co_t nonullcaller;
static co_t executor;
/* _Thread_local  */ co_t *co_self;

static __attribute__((naked))
void
co_trampoline(void)
{
	__asm__(
			/* Return value constructed from co_resume() for
			 * co_yield() must be transformed to the first
			 * argument of the coroutine procedure by the first
			 * call. */
			"mov %%rax,%%rdi\n\t"
			"ret\n\t"
		::
	);
	__builtin_unreachable();
}

int
co_create(co_t *c, void *proc(void *))
{
	void *their_stack = c->stack;
	/* assert(!((uintptr_t)their_stack % 16)); */
	void *saved_stack;
	__asm__(
			"mov %%rsp,%[saved_stack]\n\t"

			/* Setup their stack and initial frame. */
			"mov %[their_stack],%%rsp\n\t"
			"push %[proc]\n\t"
			"lea %[trampoline],%%rax\n\t"
			"push %%rax\n\t"

			/* co_resume will restore garbage but it does not mean
			 * any security breaches. */
			"sub %[nsaved],%%rsp\n\t"
			"mov %%rsp,%[their_stack]\n\t" /* Save top. */

			"mov %[saved_stack],%%rsp\n\t"
			: [saved_stack] "=r"(saved_stack),
			  [their_stack] "=&r"(their_stack)
			: [proc] "m"(proc),
			  [trampoline] "m"(co_trampoline),
			  [nsaved] "i"(
#define xmacro(type, name) +sizeof(type)
				CALLEE_SAVED
#undef xmacro
			)
	);
	c->frame = their_stack;
	return 0;
}

void
co_destroy(co_t *c)
{
	munmap((void *)c->stack, c->stack_size);
}

#if 0
open_stream()
{

	co_yield(999); -> co_resume(&co_self->caller, 999);
	co_resume(&executor, NULL);
}
#endif

/* Wake up co_t from suspension. */
__attribute__((naked))
void *
co_resume(co_t *c, void *arg)
{
#if 0
#define xmacro(type, name) { register long reg##name asm(#name); c->ctx.name = reg##name; }
	CALLEE_SAVED
#undef xmacro
#endif

	printf("from %s (frame=%p) resume %s (frame=%p) with %p\n", co_self->name, co_self->frame, c->name, c->frame, arg);

	{
		register void *rsp asm("rsp");
		printf("    rsp=%p\n", rsp);
	}


	__asm__(
			/* Save our registers. */
#define xmacro(type, name) "push %%"#name"\n\t"
			CALLEE_SAVED
#undef xmacro
			"mov %%rsp,%[our_frame]\n\t"

			/* Update references. */
			"mov %[in_self],%[caller]\n\t"
			"mov %[c],%[out_self]\n\t"

			/* Move to their frame. */
			"mov %[frame],%%rsp\n\t"

			/* Return passed value. */
			"mov %[arg],%%rax\n\t"

			/**
			 * IMPORTANT: It is not safe to put instructions below
			 * pop unless callee saved registers are among clobbed.
			 */

			/* Restore their registers. */
#define xmacro(type, name) "pop %%"#name"\n\t"
			CALLEE_SAVED
#undef xmacro
			"int3\n"

			"ret\n\t"
			: [our_frame] "=m"(co_self->frame),
			  [out_self] "=m"(co_self),
			  [caller] "=m"(c->caller)
			: [frame] "r"(c->frame),
			  [in_self] "r"(co_self),
			  [c] "r"(c),
			  [arg] "r"(arg)

			: "rax");
	__builtin_unreachable();

#if 0
#define xmacro(type, name) #name,
	__asm__ inline("":::
		CALLEE_SAVED
		"cc"
	);
#undef xmacro
	return arg;

#ifdef __x86_64__
	__asm__ inline(
			"jmp *.Lreturn%=(%%rip)\n\t"
#define xmacro(type, name) "movq %%"#name",%["#name"]\n\t"
			/* CALLEE_SAVED */
#undef xmacro

#define xmacro(type, name) "movq %["#name"],%%"#name"\n\t"
			CALLEE_SAVED
#undef xmacro
			"movq %%rax,%[arg]\n\t"
			"jmp *%[eip]\n\t"
			".Lreturn%=:\n\t"
			"ret\n\t"
			: /* Outputs. */
#define xmacro(type, name) [name] "=m" (c->ctx.name),
			CALLEE_SAVED
#undef xmacro
			[dummy] "=r"(c) /* Just because of shit trailing comma. */
			: /* Inputs. */
			[eip] "rm"(c->ctx.eip),
			[arg] "rm"(arg)
			: /* Clobbers. */
			"rax"
			);
#endif
	__builtin_unreachable();
#endif
}

/*
 * Fast:
 * -> set EIP
 * save registers onto stack
 *
 * do something short stuff
 *
 * restore registers from stack (compiler will optimize to only care about what changed)
 * <- restore EIP
*/

void *
co_yield(void *arg)
{
	printf("yield from %s to %s\n", co_self->name, co_self->caller->name);
	void *ret =  co_resume(co_self->caller, arg);
	printf("yeee\n");
	return ret;
}

void *coproc(void *arg)
{
	printf("deadbeef=%p\n", arg);
	printf("self=%p\n", co_self);
	printf("got=%p\n", co_yield((void *)0xbabe));
	return NULL;
}

static void
co_set_name(co_t *c, char const *name)
{
	strncpy(c->name, name, sizeof c->name);
}

char stack[2560]/* __attribute((aligned(8)))*/;

static co_t *ptest;
int main()
{
	executor.caller = &nonullcaller;
	co_self = &executor;

	co_set_name(&executor, "executor");

	memset(stack, 0xcd, sizeof stack);

	co_t test;
	ptest = &test;
	test.stack = (&stack)[1];
	test.stack_size = sizeof stack;
	co_create(&test, coproc/*, (void *)0xbeef*/);
	co_set_name(&test, "test");

	{
		register void *rsp asm("rbp");
		printf(">>> MAIN rsp=%p\n", rsp);
	}

	co_resume(&test, (void *)0xdeadbeef);

	{
		register void *rsp asm("rbp");
		printf(">>> MAIN rsp=%p\n", rsp);
	}

	__asm__("int3");
	co_resume(&test, (void *)0x777);

	return 0;
}

