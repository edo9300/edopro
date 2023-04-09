#ifndef __CONFIG_H
#define __CONFIG_H

#include "ocgapi_types.h"
#include "text_types.h"

extern uint16_t PRO_VERSION;
extern bool is_from_discord;
extern bool open_file;
extern epro::path_string open_file_name;
extern bool show_changelog;

#ifdef _MSC_VER
#define unreachable() __assume(0)
#else
#define unreachable() __builtin_unreachable()
#endif

#define EDOPRO_VERSION_MAJOR 40
#define EDOPRO_VERSION_MINOR 1
#define EDOPRO_VERSION_PATCH 3
#define EDOPRO_VERSION_CODENAME "Puppet of Strings"
#define EDOPRO_VERSION_STRING_DEBUG "EDOPro version " STR(EDOPRO_VERSION_MAJOR) "." STR(EDOPRO_VERSION_MINOR) "." STR(EDOPRO_VERSION_PATCH)
#define EDOPRO_VERSION_STRING L"Project Ignis: EDOPro | " STR(EDOPRO_VERSION_MAJOR) "." STR(EDOPRO_VERSION_MINOR) "." STR(EDOPRO_VERSION_PATCH) " \"" EDOPRO_VERSION_CODENAME "\""
#define CLIENT_VERSION ((EDOPRO_VERSION_MAJOR & 0xff) | ((EDOPRO_VERSION_MINOR & 0xff) << 8) | ((OCG_VERSION_MAJOR & 0xff) << 16) | ((OCG_VERSION_MINOR & 0xff) << 24))
#define GET_CLIENT_VERSION_MAJOR(ver) (ver & 0xff)
#define GET_CLIENT_VERSION_MINOR(ver) ((ver >> 8) & 0xff)
#define GET_CORE_VERSION_MAJOR(ver) ((ver >> 16) & 0xff)
#define GET_CORE_VERSION_MINOR(ver) ((ver >> 24) & 0xff)
#define EXPAND_VERSION(ver) GET_CLIENT_VERSION_MAJOR(ver), GET_CLIENT_VERSION_MINOR(ver), GET_CORE_VERSION_MAJOR(ver), GET_CORE_VERSION_MINOR(ver)

#define EDOPRO_WINDOWS 0
#define EDOPRO_LINUX 0
#define EDOPRO_ANDROID 0
#define EDOPRO_IOS 0
#define EDOPRO_MACOS 0

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IOS == 1
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
#define OSSTRING "Linux"
#elif EDOPRO_ANDROID
#define OSSTRING "Android"
#endif
#define EDOPRO_APPLE (EDOPRO_IOS || EDOPRO_MACOS)
#define EDOPRO_LINUX_KERNEL (EDOPRO_LINUX || EDOPRO_ANDROID)

#endif
