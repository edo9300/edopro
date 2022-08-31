#include <WinSock2.h>
#include <Windows.h>
#include <cstdlib>
#include <memory>

#define KERNELEX 1

/*
creates 2 functions, the stub function prefixed by handledxxx that is then exported via asm,
and internalimplxxx that is the fallback function called if the function isn't loaded at runtime
on first call GetProcAddress is called, and if the function is found, then that will be called onwards
otherwise fall back to the internal implementation
*/

#define GETFUNC(funcname) (decltype(&handled##funcname))GetProcAddress(GetModuleHandle(LIBNAME), #funcname)
#define MAKELOADER(funcname,ret,args,argnames) \
ret __stdcall internalimpl##funcname args ; \
extern "C" ret __stdcall handled##funcname args; \
const auto basefunc##funcname = [] { \
	auto func = GETFUNC(funcname); \
	return func ? func : internalimpl##funcname; \
}(); \
extern "C" ret __stdcall handled##funcname args { \
	return basefunc##funcname argnames ; \
} \
ret __stdcall internalimpl##funcname args

#define MAKELOADER_WITH_CHECK(funcname,ret,args,argnames) \
ret __stdcall internalimpl##funcname args ; \
extern "C" ret __stdcall handled##funcname args; \
const auto basefunc##funcname = [] { \
	auto func = GETFUNC(funcname); \
	return func ? func : internalimpl##funcname; \
}(); \
extern "C" ret __stdcall handled##funcname args { \
	if(!basefunc##funcname) { \
		auto func = GETFUNC(funcname); \
		return (func ? func : internalimpl##funcname) argnames ; \
	} \
	return basefunc##funcname argnames; \
} \
ret __stdcall internalimpl##funcname args

#define MAKELOADERCRT(funcname,ret,args,argnames) \
ret cdecl internalimpl##funcname args ; \
extern "C" ret cdecl handled##funcname args; \
const auto basefunc##funcname = [] { \
	auto func = GETFUNC(funcname); \
	return func ? func : internalimpl##funcname; \
}(); \
extern "C" ret cdecl handled##funcname args { \
	return basefunc##funcname argnames ; \
} \
ret cdecl internalimpl##funcname args

#ifndef _WIN64
#ifdef _MSC_VER
#define socklen_t int
#endif
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
#include <winternl.h>
#endif

namespace {
//some implementations taken from https://sourceforge.net/projects/win2kxp/

#if 0
//can't use c runtime functions as the runtime might not have been loaded yet
void ___write(const char* ch) {
	DWORD dwCount;
	static HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsoleA(hOut, ch, strlen(ch), &dwCount, nullptr);
}
void ___write(const wchar_t* ch) {
	DWORD dwCount;
	static HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsoleW(hOut, ch, wcslen(ch), &dwCount, nullptr);
}
#endif


const auto pfFreeAddrInfo = (WSPIAPI_PFREEADDRINFO)WspiapiLoad(2);
extern "C" void __stdcall handledfreeaddrinfo(addrinfo * ai) {
	pfFreeAddrInfo(ai);
}

const auto pfGetAddrInfo = (WSPIAPI_PGETADDRINFO)WspiapiLoad(0);
extern "C" INT __stdcall handledgetaddrinfo(const char* nodename, const char* servname, const addrinfo* hints, addrinfo** res) {
	auto iError = pfGetAddrInfo(nodename, servname, hints, res);
	WSASetLastError(iError);
	return iError;
}

const auto pfGetNameInfo = (WSPIAPI_PGETNAMEINFO)WspiapiLoad(1);
extern "C" INT __stdcall handledgetnameinfo(const sockaddr* sa, socklen_t salen, char* host, size_t hostlen, char* serv, size_t servlen, int flags) {
	const auto iError = pfGetNameInfo(sa, salen, host, hostlen, serv, servlen, flags);
	WSASetLastError(iError);
	return iError;
}

const bool kernelex = GetModuleHandle(__TEXT("ntdll.dll")) == nullptr;
inline bool IsUnderKernelex() {
	//ntdll.dll is loaded automatically in every windows nt process, but it seems it isn't in windows 9x
	return kernelex;
}

#define LIBNAME __TEXT("Advapi32.dll")

#if KERNELEX


#define MAKELOADER_KERNELEX(funcname,ret,args,argnames) \
ret __stdcall kernelex##funcname args ; \
extern "C" ret __stdcall handled##funcname args; \
const auto basefunc##funcname = [] { \
	if (IsUnderKernelex()) \
		return kernelex##funcname; \
	else \
		return GETFUNC(funcname); \
}(); \
extern "C" ret __stdcall handled##funcname args { \
	return basefunc##funcname argnames ; \
} \
ret __stdcall kernelex##funcname args

MAKELOADER_KERNELEX(CryptAcquireContextW, BOOL, (HCRYPTPROV* phProv, LPCWSTR pszContainer, LPCWSTR pszProvider, DWORD dwProvType, DWORD dwFlags),
					(phProv, pszContainer, pszProvider, dwProvType, dwFlags)) {
	return pszContainer == nullptr && pszProvider == nullptr;
}

MAKELOADER_KERNELEX(CryptGenRandom, BOOL, (HCRYPTPROV hProv, DWORD dwLen, BYTE* pbBuffer),
					(hProv, dwLen, pbBuffer)) {
	auto RtlGenRandom = (BOOLEAN(__stdcall*)(PVOID RandomBuffer, ULONG RandomBufferLength))GetProcAddress(GetModuleHandle(LIBNAME), "SystemFunction036");
	return RtlGenRandom && RtlGenRandom(pbBuffer, dwLen);
}

MAKELOADER_KERNELEX(CryptReleaseContext, BOOL, (HCRYPTPROV hProv, DWORD dwFlags), (hProv, dwFlags)) {
	return TRUE;
}
#endif

#undef LIBNAME
#define LIBNAME __TEXT("kernel32.dll")

typedef struct _LDR_MODULE {
	LIST_ENTRY InLoadOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
	PVOID BaseAddress;
	PVOID EntryPoint;
	ULONG SizeOfImage;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG Flags;
	SHORT LoadCount;
	SHORT TlsIndex;
	LIST_ENTRY HashTableEntry;
	ULONG TimeDateStamp;
} LDR_MODULE, *PLDR_MODULE;

typedef struct _PEB_LDR_DATA_2K {
	ULONG Length;
	BOOLEAN Initialized;
	PVOID SsHandle;
	LIST_ENTRY InLoadOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA_2K, *PPEB_LDR_DATA_2K;

typedef struct _RTL_USER_PROCESS_PARAMETERS_2K {
	ULONG MaximumLength;
	ULONG Length;
	ULONG Flags;
	ULONG DebugFlags;
	PVOID ConsoleHandle;
	ULONG ConsoleFlags;
	HANDLE StdInputHandle;
	HANDLE StdOutputHandle;
	HANDLE StdErrorHandle;
} RTL_USER_PROCESS_PARAMETERS_2K, *PRTL_USER_PROCESS_PARAMETERS_2K;
typedef struct _PEB_2K {
	BOOLEAN InheritedAddressSpace;
	BOOLEAN ReadImageFileExecOptions;
	BOOLEAN BeingDebugged;
	BOOLEAN Spare;
	HANDLE Mutant;
	PVOID ImageBaseAddress;
	PPEB_LDR_DATA_2K LoaderData;
	PRTL_USER_PROCESS_PARAMETERS_2K ProcessParameters;
	PVOID SubSystemData;
	PVOID ProcessHeap;
	//And more but we don't care...
	DWORD reserved[0x21];
	PRTL_CRITICAL_SECTION LoaderLock;
} PEB_2K, *PPEB_2K;

typedef struct _CLIENT_ID {
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
} CLIENT_ID;
typedef CLIENT_ID *PCLIENT_ID;

typedef struct _TEB_2K {
	NT_TIB NtTib;
	PVOID EnvironmentPointer;
	CLIENT_ID Cid;
	PVOID ActiveRpcInfo;
	PVOID ThreadLocalStoragePointer;
	PPEB_2K Peb;
	DWORD reserved[0x3D0];
	BOOLEAN InDbgPrint; // 0xF74
	BOOLEAN FreeStackOnTermination;
	BOOLEAN HasFiberData;
	UCHAR IdealProcessor;
} TEB_2K, *PTEB_2K;
inline PTEB_2K NtCurrentTeb2k(void) {
	return (PTEB_2K)NtCurrentTeb();
}

inline PPEB_2K NtCurrentPeb(void) {
	return NtCurrentTeb2k()->Peb;
}
PLDR_MODULE __stdcall GetLdrModule(LPCVOID address) {
	PLDR_MODULE first_mod, mod;
	first_mod = mod = (PLDR_MODULE)NtCurrentPeb()->LoaderData->InLoadOrderModuleList.Flink;
	do {
		if((ULONG_PTR)mod->BaseAddress <= (ULONG_PTR)address &&
			(ULONG_PTR)address < (ULONG_PTR)mod->BaseAddress + mod->SizeOfImage)
			return mod;
		mod = (PLDR_MODULE)mod->InLoadOrderModuleList.Flink;
	} while(mod != first_mod);
	return nullptr;
}

void LoaderLock(BOOL lock) {
	if(lock)
		EnterCriticalSection(NtCurrentPeb()->LoaderLock);
	else
		LeaveCriticalSection(NtCurrentPeb()->LoaderLock);
}
HMODULE GetModuleHandleFromPtr(LPCVOID p) {
	PLDR_MODULE pLM;
	HMODULE ret;
	LoaderLock(TRUE);
	pLM = GetLdrModule(p);
	if(pLM)
		ret = (HMODULE)pLM->BaseAddress;
	else
		ret = nullptr;
	LoaderLock(FALSE);
	return ret;
}

MAKELOADER(ConvertFiberToThread, BOOL, (), ()) {
	PVOID Fiber;
	if(NtCurrentTeb2k()->HasFiberData) {
		NtCurrentTeb2k()->HasFiberData = FALSE;
		Fiber = NtCurrentTeb2k()->NtTib.FiberData;
		if(Fiber) {
			NtCurrentTeb2k()->NtTib.FiberData = nullptr;
			HeapFree(GetProcessHeap(), 0, Fiber);
		}
		return TRUE;
	} else {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
}

void IncLoadCount(HMODULE hMod) {
	WCHAR path[MAX_PATH];
	if(GetModuleFileNameW(hMod, path, sizeof(path)) != sizeof(path))
		LoadLibraryW(path);
}

MAKELOADER(GetModuleHandleExW, BOOL, (DWORD dwFlags, LPCWSTR lpModuleName, HMODULE* phModule), (dwFlags, lpModuleName, phModule)) {
	LoaderLock(TRUE);
	if(dwFlags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)
		*phModule = GetModuleHandleFromPtr(lpModuleName);
	else
		*phModule = GetModuleHandleW(lpModuleName);

	if(*phModule == nullptr) {
		LoaderLock(FALSE);
		return FALSE;
	}
	if(!(dwFlags & GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT) ||
	   dwFlags & GET_MODULE_HANDLE_EX_FLAG_PIN) {
		//How to pin? We'll just inc the LoadCount and hope
		IncLoadCount(*phModule);
	}

	LoaderLock(FALSE);
	return TRUE;
}

MAKELOADER(AddVectoredExceptionHandler, PVOID, (ULONG First, PVECTORED_EXCEPTION_HANDLER Handler), (First, Handler)) {
	return NULL;
}

MAKELOADER(RemoveVectoredExceptionHandler, ULONG, (PVOID Handle), (Handle)) {
	return 0;
}

#undef LIBNAME
#define LIBNAME __TEXT("kernel32.dll")
/* We need the initial tick count to detect if the tick
 * count has rolled over. */
DWORD initial_tick_count = GetTickCount();
MAKELOADER(GetTickCount64, ULONGLONG, (), ()) {
	/* GetTickCount returns the number of milliseconds that have
	 * elapsed since the system was started. */
	DWORD count = GetTickCount();
	if(count < initial_tick_count) {
		/* The tick count has rolled over - adjust for it. */
		count = (0xFFFFFFFFu - initial_tick_count) + count;
	}
	return static_cast<DWORD>(static_cast<double>(count) / 1000.0);
}
#undef LIBNAME
#define LIBNAME __TEXT("msvcrt.dll")
MAKELOADERCRT(_aligned_malloc, void*, (size_t size, size_t alignment), (size, alignment)) {
	return __mingw_aligned_malloc(size, alignment);
}
MAKELOADERCRT(_aligned_free, void, (void* ptr), (ptr)) {
	return __mingw_aligned_free(ptr);
}

using qsortfunc = int(__cdecl*)(void*, const void*, const void*);
struct Sorter {
	qsortfunc m_callback{};
	void* m_ptr;
};
thread_local Sorter sorter;

static int cdecl SortCallback(void const* a, void const* b) {
	return sorter.m_callback(sorter.m_ptr, a, b);
}

MAKELOADERCRT(qsort_s, void, (void* base, size_t num, size_t width, qsortfunc compare, void* context), (base, num, width, compare, context)) {
	sorter.m_callback = compare;
	sorter.m_ptr = context;
	qsort(base, num, width, SortCallback);
}

extern "C" ULONG __stdcall if_nametoindex(PCSTR* InterfaceName) {
	return 0;
}
}
