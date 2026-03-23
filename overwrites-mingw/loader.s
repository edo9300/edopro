.macro DECLARE_STUB_INT functionname, symbolname
	.global __imp__\functionname
	.global _\symbolname\()
.section .text
	\symbolname\()aa:
		call _LoadSymbols@0
		jmp *__imp__\functionname
.section .data
	__imp__\functionname\(): .long \symbolname\()aa
	_\symbolname\(): .long __imp__\functionname\()
.endm

.macro DECLARE_STUB functionname, parameters
	DECLARE_STUB_INT \functionname\()@\parameters, \functionname\()symbol
.endm

.macro DECLARE_STUB_CRT functionname
	DECLARE_STUB_INT \functionname\(), \functionname\()symbol
.endm

DECLARE_STUB AddVectoredExceptionHandler, 8
DECLARE_STUB RemoveVectoredExceptionHandler, 4
DECLARE_STUB ConvertFiberToThread, 0
DECLARE_STUB GetTickCount64, 0
DECLARE_STUB GetModuleHandleExW, 12
DECLARE_STUB if_nametoindex, 4
DECLARE_STUB freeaddrinfo, 4
DECLARE_STUB getaddrinfo, 16
DECLARE_STUB getnameinfo, 28
DECLARE_STUB CryptAcquireContextW, 20
DECLARE_STUB CryptGenRandom, 12
DECLARE_STUB CryptReleaseContext, 8

DECLARE_STUB_CRT _aligned_malloc
DECLARE_STUB_CRT _aligned_free
DECLARE_STUB_CRT qsort_s
DECLARE_STUB_CRT fopen_s
