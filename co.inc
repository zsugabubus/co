/* Proper. */
#define CO_FAST 0
#define CO_MAKE_NAME(_, x) x
#define CO_CALLEE_SAVED CO_CLOBBERED
#include CO_INCLUDE_FILE
#undef CO_CALLEE_SAVED
#undef CO_MAKE_NAME
#undef CO_FAST

/* Caller saved calle-saved. */
#define CO_FAST 1
#define CO_MAKE_NAME(_, x) _##x##_fast
#define CO_CALLEE_SAVED(order) /* Forced to be saved by caller. */
#include CO_INCLUDE_FILE
#undef CO_CALLEE_SAVED
#undef CO_MAKE_NAME
#undef CO_FAST

#undef CO_INCLUDE_FILE

/* vim: set ft=c: */
