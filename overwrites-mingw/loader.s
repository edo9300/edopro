.macro DECLARE_STUB_INT functionname, symbolname
	.global __imp__\functionname
	.global _\symbolname\()
.section .text
	1:
		call _LoadSymbols@0
		jmp *__imp__\functionname
.section .data
	__imp__\functionname\(): .long 1b
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
DECLARE_STUB AttachConsole, 4
DECLARE_STUB VerSetConditionMask, 16
DECLARE_STUB VerifyVersionInfoA, 16
DECLARE_STUB GetLongPathNameW, 12
DECLARE_STUB IsWellKnownSid, 8
DECLARE_STUB CheckTokenMembership, 12
DECLARE_STUB CryptEnumProvidersW, 24

DECLARE_STUB AcquireCredentialsHandleW, 36
DECLARE_STUB CompleteAuthToken, 8
DECLARE_STUB InitializeSecurityContextW, 48
DECLARE_STUB QuerySecurityPackageInfoW, 8
DECLARE_STUB DeleteSecurityContext, 4
DECLARE_STUB FreeContextBuffer, 4
DECLARE_STUB FreeCredentialsHandle, 4

DECLARE_STUB_CRT _aligned_malloc
DECLARE_STUB_CRT _aligned_free
DECLARE_STUB_CRT qsort_s
DECLARE_STUB_CRT fopen_s
