.global __imp__AddVectoredExceptionHandler@8
__imp__AddVectoredExceptionHandler@8: .long _handledAddVectoredExceptionHandler@8

.global __imp__RemoveVectoredExceptionHandler@4
__imp__RemoveVectoredExceptionHandler@4: .long _handledRemoveVectoredExceptionHandler@4

.global __imp__ConvertFiberToThread@0
__imp__ConvertFiberToThread@0: .long _handledConvertFiberToThread@0

.global __imp__GetTickCount64@0
__imp__GetTickCount64@0: .long _handledGetTickCount64@0

.global __imp__GetModuleHandleExW@12
__imp__GetModuleHandleExW@12: .long _handledGetModuleHandleExW@12

.global __imp___aligned_malloc
__imp___aligned_malloc: .long _handled_aligned_malloc

.global __imp___aligned_free
__imp___aligned_free: .long _handled_aligned_free

.global __imp__qsort_s
__imp__qsort_s: .long _handledqsort_s

.global __imp__freeaddrinfo@4
__imp__freeaddrinfo@4: .long _handledfreeaddrinfo@4

.global __imp__getaddrinfo@16
__imp__getaddrinfo@16: .long _handledgetaddrinfo@16

.global __imp__getnameinfo@28
__imp__getnameinfo@28: .long _handledgetnameinfo@28

.global __imp__CryptAcquireContextW@20
__imp__CryptAcquireContextW@20: .long _handledCryptAcquireContextW@20

.global __imp__CryptGenRandom@12
__imp__CryptGenRandom@12: .long _handledCryptGenRandom@12

.global __imp__CryptReleaseContext@8
__imp__CryptReleaseContext@8: .long _handledCryptReleaseContext@8
