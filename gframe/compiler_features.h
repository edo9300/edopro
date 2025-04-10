#ifndef COMPILER_FEATURES_H
#define COMPILER_FEATURES_H

#if defined( _MSC_VER) && !defined(__clang_analyzer__)
#define unreachable() __assume(0)
#define NoInline __declspec(noinline)
#define ForceInline __forceinline
#else
#if !defined(__forceinline)
#define ForceInline __attribute__((always_inline)) inline
#else
#define ForceInline __forceinline
#endif
#define unreachable() __builtin_unreachable()
#define NoInline __attribute__ ((noinline))
#endif

#define EDOPRO_WINDOWS 0
#define EDOPRO_LINUX 0
#define EDOPRO_ANDROID 0
#define EDOPRO_IOS 0
#define EDOPRO_IOS_SIMULATOR 0
#define EDOPRO_MACOS 0

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_SIMULATOR == 1
#undef EDOPRO_IOS_SIMULATOR
#define EDOPRO_IOS_SIMULATOR 1
#undef EDOPRO_IOS
#define EDOPRO_IOS 1
#elif TARGET_OS_IOS == 1
#undef EDOPRO_IOS
#define EDOPRO_IOS 1
#else
#undef EDOPRO_MACOS
#define EDOPRO_MACOS 1
#endif
#endif //__APPLE__

#if defined(__linux__)
#if defined(__ANDROID__)
#undef EDOPRO_ANDROID
#define EDOPRO_ANDROID 1
#else
#undef EDOPRO_LINUX
#define EDOPRO_LINUX 1
#endif
#endif

#if defined(_WIN32)
#undef EDOPRO_WINDOWS
#define EDOPRO_WINDOWS 1
#endif

#if EDOPRO_WINDOWS
#define OSSTRING "Windows"
#elif EDOPRO_MACOS
#define OSSTRING "Mac"
#elif EDOPRO_IOS
#define OSSTRING "iOS"
#elif EDOPRO_LINUX
#if defined(__aarch64__)
#define OSSTRING "Linux AArch64"
#else
#define OSSTRING "Linux"
#endif
#elif EDOPRO_ANDROID
#define OSSTRING "Android"
#endif
#define EDOPRO_APPLE (EDOPRO_IOS || EDOPRO_MACOS)
#define EDOPRO_LINUX_KERNEL (EDOPRO_LINUX || EDOPRO_ANDROID)
#define EDOPRO_POSIX (EDOPRO_LINUX_KERNEL || EDOPRO_APPLE)

#endif
