# Coroutine

Simple yet powerful (a)symmetric stackful and stackless coroutine
implementation in C targeting GNU C Compiler, System V ABI, AMD64.

Stackful coroutines provide register-based communication channel; meaningful
backtrace; have 16 bytes minimum overhead; and optionally can take advantage of
caller-saved callee-saved-register saving for minimal overhead–thus achieving
theoretically possible maximum performance.

Stackless coroutines provide branchless context switching with overhead that
maybe as small as a single byte (depending on code size).

## License

Released under the GNU General Public License version v3.0 or later.
