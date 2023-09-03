#ifndef _WIN64
#include <WinSock2.h>
#endif
#include <Windows.h>
#include <cstdint>
#include <array>
#include <utility>

namespace {

template<typename T, typename T2>
inline T function_cast(T2 ptr) {
	using generic_function_ptr = void (*)(void);
	return reinterpret_cast<T>(reinterpret_cast<generic_function_ptr>(ptr));
}

namespace Detail {
struct Args {
	FARPROC*& targetSymbol;
	FARPROC(__stdcall* load)();
};

#ifndef __INTELLISENSE__

template<size_t N>
struct Callable;


#define STUB_WITH_CALLABLE_INT(index,funcname,...) \
extern "C" FARPROC* funcname##symbol; \
template<> \
struct Detail::Callable<index> { \
	static constexpr auto& reference_symbol{funcname##symbol}; \
	static auto __stdcall load() { \
		return reinterpret_cast<FARPROC>(__VA_ARGS__); \
	} \
};

#define STUB_WITH_CALLABLE(funcname,...) STUB_WITH_CALLABLE_INT(__COUNTER__, funcname,__VA_ARGS__)


#define STUB_WITH_LIBRARY_INT(index,funcname,ret,args,libraryName) \
extern "C" FARPROC* funcname##symbol; \
ret __stdcall internalimpl##funcname args ; \
template<> \
struct Detail::Callable<index> { \
	static constexpr auto& reference_symbol{funcname##symbol}; \
	static auto __stdcall load() { \
		const auto loaded = function_cast<FARPROC>(GetProcAddress(GetModuleHandle(libraryName), #funcname)); \
		return loaded ? loaded : function_cast<FARPROC>(internalimpl##funcname); \
	} \
}; \
ret __stdcall internalimpl##funcname args

#define STUB_WITH_LIBRARY(funcname,ret,args) STUB_WITH_LIBRARY_INT(__COUNTER__, funcname,ret,args, LIBNAME)

template<size_t... I>
constexpr auto make_overridden_functions_array(std::index_sequence<I...> seq) {
	return std::array<Args, seq.size()>{Args{ Detail::Callable<I>::reference_symbol, Detail::Callable<I>::load }...};
}

#define GET_OVERRIDDEN_FUNCTIONS_ARRAY() \
	Detail::make_overridden_functions_array(std::make_index_sequence<__COUNTER__>());

#else
#define STUB_WITH_CALLABLE(funcname,...)
#define STUB_WITH_LIBRARY(funcname,ret,args) ret __stdcall internalimpl##funcname args
template<size_t... I>
constexpr auto make_overridden_functions_array() {
	return std::array<Args, 0>{};
}
#define GET_OVERRIDDEN_FUNCTIONS_ARRAY() Detail::make_overridden_functions_array();
#endif

} // namespace Detail
} // namespace



#ifndef _WIN64
#define socklen_t int
#define EAI_AGAIN           WSATRY_AGAIN
#define EAI_BADFLAGS        WSAEINVAL
#define EAI_FAIL            WSANO_RECOVERY
#define EAI_FAMILY          WSAEAFNOSUPPORT
#define EAI_MEMORY          WSA_NOT_ENOUGH_MEMORY
#define EAI_NOSECURENAME    WSA_SECURE_HOST_NOT_FOUND
#define EAI_NONAME          WSAHOST_NOT_FOUND
#define EAI_NODATA          EAI_NONAME
#define EAI_SERVICE         WSATYPE_NOT_FOUND
#define EAI_SOCKTYPE        WSAESOCKTNOSUPPORT
#define EAI_IPSECPOLICY     WSA_IPSEC_NAME_POLICY_ERROR
#include <WSPiApi.h>
#undef freeaddrinfo
#undef getaddrinfo
#undef getnameinfo
#endif

namespace {
#ifndef _WIN64

STUB_WITH_CALLABLE(freeaddrinfo, WspiapiLoad(2))
STUB_WITH_CALLABLE(getaddrinfo, WspiapiLoad(0))
STUB_WITH_CALLABLE(getnameinfo, WspiapiLoad(1))

#undef LIBNAME
#define LIBNAME __TEXT("kernel32.dll")

bool GetRealOSVersion(LPOSVERSIONINFOW lpVersionInfo) {
	auto GetWin9xProductInfo = [lpVersionInfo] {
		WCHAR data[80];
		HKEY hKey;
		auto GetRegEntry = [&data, &hKey](const WCHAR* name) {
			DWORD dwRetFlag, dwBufLen{ sizeof(data) };
			return (RegQueryValueExW(hKey, name, nullptr, &dwRetFlag, (LPBYTE)data, &dwBufLen) == ERROR_SUCCESS)
				&& (dwRetFlag & REG_SZ) != 0;
		};
		if(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
			return false;
		if(GetRegEntry(L"VersionNumber") && data[0] == L'4') {
			lpVersionInfo->dwPlatformId = VER_PLATFORM_WIN32_WINDOWS;
			lpVersionInfo->dwMajorVersion = 4;
			if(data[2] == L'9') {
				lpVersionInfo->dwMinorVersion = 90;
			} else {
				lpVersionInfo->dwMinorVersion = data[2] == L'0' ? 0 : 10;
				if(GetRegEntry(L"SubVersionNumber"))
					lpVersionInfo->szCSDVersion[1] = data[1];
			}
			RegCloseKey(hKey);
			return true;
		}
		RegCloseKey(hKey);
		return false;
	};
	auto GetWindowsVersionNotCompatMode = [lpVersionInfo] {
		using RtlGetVersionPtr = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);
		const auto func = function_cast<RtlGetVersionPtr>(GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "RtlGetVersion"));
		if(func && func(lpVersionInfo) == 0x00000000) {
			if(lpVersionInfo->dwMajorVersion != 5 || lpVersionInfo->dwMinorVersion != 0)
				return true;
		}
		return !!(function_cast<decltype(&GetVersionExW)>(GetProcAddress(GetModuleHandle(LIBNAME), "GetVersionExW")))(lpVersionInfo);
	};
#if KERNELEX
	if(IsUnderKernelex())
		return GetWin9xProductInfo();
#endif
	return GetWindowsVersionNotCompatMode();
}

const OSVERSIONINFOEXW system_version = []() {
	OSVERSIONINFOEXW ret{};
	ret.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
	if(!GetRealOSVersion(reinterpret_cast<OSVERSIONINFOW*>(&ret))) {
		ret.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
		if(!GetRealOSVersion(reinterpret_cast<OSVERSIONINFOW*>(&ret))) {
			ret.dwOSVersionInfoSize = 0;
			return ret;
		}
	}
	return ret;
}();

/*
* first call will be from irrlicht, no overwrite, 2nd will be from the crt,
* if not spoofed, the runtime will abort as windows 2k isn't supported
*/
int firstrun = 0;
extern "C" BOOL __stdcall internalimpGetVersionExW(LPOSVERSIONINFOW lpVersionInfo) {
	if(!lpVersionInfo
	   || (lpVersionInfo->dwOSVersionInfoSize != sizeof(OSVERSIONINFOEXW) && lpVersionInfo->dwOSVersionInfoSize != sizeof(OSVERSIONINFOW))
	   || (lpVersionInfo->dwOSVersionInfoSize > system_version.dwOSVersionInfoSize)) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	memcpy(lpVersionInfo, &system_version, lpVersionInfo->dwOSVersionInfoSize);
	//spoof win2k to the c runtime
	if(firstrun == 1 && lpVersionInfo->dwMajorVersion <= 5) {
		firstrun++;
		lpVersionInfo->dwMajorVersion = 5; //windows xp
		lpVersionInfo->dwMinorVersion = 1;
	} else if(firstrun == 0)
		firstrun++;
	return TRUE;
}
STUB_WITH_CALLABLE(GetVersionExW, internalimpGetVersionExW)

// We take the small memory leak and we map Fls functions to Tls
// that don't support the callback function

STUB_WITH_LIBRARY(FlsAlloc, DWORD, (PFLS_CALLBACK_FUNCTION lpCallback)) {
	return TlsAlloc();
}

STUB_WITH_LIBRARY(FlsSetValue, BOOL, (DWORD dwFlsIndex, PVOID lpFlsData)) {
	return TlsSetValue(dwFlsIndex, lpFlsData);
}

STUB_WITH_LIBRARY(FlsGetValue, PVOID, (DWORD dwFlsIndex)) {
	return TlsGetValue(dwFlsIndex);
}

STUB_WITH_LIBRARY(FlsFree, BOOL, (DWORD dwFlsIndex)) {
	return TlsFree(dwFlsIndex);
}
#endif

#undef LIBNAME
#define LIBNAME __TEXT("kernel32.dll")
/* We need the initial tick count to detect if the tick
 * count has rolled over. */
DWORD initial_tick_count = GetTickCount();
STUB_WITH_LIBRARY(GetTickCount64, ULONGLONG, ()) {
	/* GetTickCount returns the number of milliseconds that have
	 * elapsed since the system was started. */
	DWORD count = GetTickCount();
	if(count < initial_tick_count) {
		/* The tick count has rolled over - adjust for it. */
		count = (0xFFFFFFFFu - initial_tick_count) + count;
	}
	return static_cast<DWORD>(static_cast<double>(count) / 1000.0);
}

ULONG __stdcall internalimplif_nametoindex(PCSTR * InterfaceName) {
	return 0;
}

STUB_WITH_CALLABLE(if_nametoindex, internalimplif_nametoindex)

extern "C" ULONG __stdcall if_nametoindex(PCSTR* InterfaceName) {
	return 0;
}

/*
* Manually replace the various __imp__ pointers with the right function
* (either the one loaded from a dll or the reimplementation)
* this relies on a symbol holding the function pointer to be exported
* from the asm file that will then be referenced in the functions array
*/
extern "C" void __stdcall LoadSymbols() {
	static constexpr auto functionArray = GET_OVERRIDDEN_FUNCTIONS_ARRAY();
	for(const auto& obj : functionArray)
		*obj.targetSymbol = obj.load();
}
}
