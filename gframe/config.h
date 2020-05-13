#ifndef __CONFIG_H
#define __CONFIG_H

#ifndef TEXT
#define TEXT(x) x
#endif

#ifdef _WIN32

#include <WinSock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#ifdef _MSC_VER
#define mywcsncasecmp _wcsnicmp
#define mystrncasecmp _strnicmp
#else
#define mywcsncasecmp wcsncasecmp
#define mystrncasecmp strncasecmp
#endif

#define socklen_t int

#else //_WIN32

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <locale.h>

#define SD_BOTH 2
#define SOCKET int
#define closesocket close
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SOCKADDR_IN sockaddr_in
#define SOCKADDR sockaddr
#define SOCKET_ERRNO() (errno)

#include <cwchar>
#define mywcsncasecmp wcsncasecmp
#define mystrncasecmp strncasecmp
inline int _wtoi(const wchar_t * s) {
	wchar_t * endptr;
	return (int)wcstol(s, &endptr, 10);
}
#endif

#ifndef TEXT
#ifdef UNICODE
#define TEXT(x) L##x
#else
#define TEXT(x) x
#endif // UNICODE
#endif

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <memory.h>
#include <ctime>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include "bufferio.h"
#include <thread>
#ifndef YGOPRO_BUILD_DLL
#include <ocgapi.h>
#else
#include "dllinterface.h"
#endif
#include <common.h>
#include "utils.h"

extern unsigned short PRO_VERSION;
extern bool exit_on_return;
extern bool open_file;
extern std::wstring open_file_name;

#define EDOPRO_VERSION_MAJOR 38
#define EDOPRO_VERSION_MINOR 1
#define EDOPRO_VERSION_PATCH 0
#define EDOPRO_VERSION_CODENAME L"Hope Harbinger"
#define CLIENT_VERSION (EDOPRO_VERSION_MAJOR & 0xff | ((EDOPRO_VERSION_MINOR & 0xff) << 8) | ((OCG_VERSION_MAJOR & 0xff) << 16) | ((OCG_VERSION_MINOR & 0xff) << 24))
#define EXPAND_VERSION(ver) (ver) & 0xff, (((ver) >> 8) & 0xff), (((ver) >> 16) & 0xff), (((ver) >> 24) & 0xff)

#endif