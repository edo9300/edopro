#ifndef _WIN64
#include <winsock2.h>
#endif
#include <windows.h>
#include <winternl.h>
#include <cstdint>
#include <array>
#include <utility>
#define SECURITY_WIN32
#include <security.h>

namespace {

template<typename T, typename T2>
inline T function_cast(T2 ptr) {
	using generic_function_ptr = void (*)(void);
	return reinterpret_cast<T>(reinterpret_cast<generic_function_ptr>(ptr));
}

#define KERNELEX 1

namespace Detail {
struct Args {
	FARPROC*& targetSymbol;
	FARPROC(__stdcall* load)();
};

#ifndef __INTELLISENSE__

template<size_t N>
struct Callable;


#define STUB_WITH_CALLABLE_INT(index,funcname,...) \
}\
extern "C" FARPROC* funcname##symbol; \
namespace {\
template<> \
struct Detail::Callable<index> { \
	static constexpr auto& reference_symbol{funcname##symbol}; \
	static auto __stdcall load() { \
		return function_cast<FARPROC>(__VA_ARGS__); \
	} \
};

#define STUB_WITH_CALLABLE(funcname,...) STUB_WITH_CALLABLE_INT(__COUNTER__, funcname,__VA_ARGS__)


#define STUB_WITH_LIBRARY_INT(index,funcname,ret,args,libraryName) \
}\
extern "C" FARPROC* funcname##symbol; \
namespace {\
ret CALLING_CONVENTION internalimpl##funcname args ; \
template<> \
struct Detail::Callable<index> { \
	static constexpr auto& reference_symbol{funcname##symbol}; \
	static auto __stdcall load() { \
		const auto loaded = function_cast<FARPROC>(GetProcAddress(GetModuleHandle(libraryName), #funcname)); \
		return loaded ? loaded : function_cast<FARPROC>(internalimpl##funcname); \
	} \
}; \
ret CALLING_CONVENTION internalimpl##funcname args

#define STUB_WITH_LIBRARY(funcname,ret,args) STUB_WITH_LIBRARY_INT(__COUNTER__, funcname,ret,args, LIBNAME)

#define STUB_KERNELEX(funcname,ret,args) \
ret CALLING_CONVENTION kernelex##funcname args ; \
	STUB_WITH_CALLABLE(funcname, []{ \
	if (IsUnderKernelex()) \
		return function_cast<FARPROC>(kernelex##funcname); \
	else \
		return function_cast<FARPROC>(GetProcAddress(GetModuleHandle(LIBNAME), #funcname)); \
	}() \
) \
ret CALLING_CONVENTION kernelex##funcname args

template<size_t... I>
constexpr auto make_overridden_functions_array(std::index_sequence<I...> seq) {
	return std::array<Args, seq.size()>{Args{ Detail::Callable<I>::reference_symbol, Detail::Callable<I>::load }...};
}

#define GET_OVERRIDDEN_FUNCTIONS_ARRAY() \
	Detail::make_overridden_functions_array(std::make_index_sequence<__COUNTER__>());

#else
#define STUB_WITH_CALLABLE(funcname,...)
#define STUB_WITH_LIBRARY(funcname,ret,args) ret CALLING_CONVENTION internalimpl##funcname args
#define STUB_KERNELEX(funcname,ret,args) ret CALLING_CONVENTION kernelex##funcname args
template<size_t... I>
constexpr auto make_overridden_functions_array() {
	return std::array<Args, 0>{};
}
#define GET_OVERRIDDEN_FUNCTIONS_ARRAY() Detail::make_overridden_functions_array();
#endif

} // namespace Detail
} // namespace



#ifndef _WIN64
// #define socklen_t int
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
#include <wspiapi.h>
#ifdef freeaddrinfo
#undef freeaddrinfo
#endif
#ifdef getaddrinfo
#undef getaddrinfo
#endif
#ifdef getnameinfo
#undef getnameinfo
#endif
#endif

#define CALLING_CONVENTION __stdcall

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
#ifndef _WIN64

STUB_WITH_CALLABLE(freeaddrinfo, WspiapiLoad(2))
STUB_WITH_CALLABLE(getaddrinfo, WspiapiLoad(0))
STUB_WITH_CALLABLE(getnameinfo, WspiapiLoad(1))

const bool kernelex = GetModuleHandle(__TEXT("ntdll.dll")) == nullptr;
inline bool IsUnderKernelex() {
	//ntdll.dll is loaded automatically in every windows nt process, but it seems it isn't in windows 9x
	return kernelex;
}

#define LIBNAME __TEXT("Advapi32.dll")
#if KERNELEX

STUB_KERNELEX(CryptAcquireContextW, BOOL, (HCRYPTPROV* phProv, LPCWSTR pszContainer, LPCWSTR pszProvider, DWORD dwProvType, DWORD dwFlags)) {
	return pszContainer == nullptr && pszProvider == nullptr;
}

STUB_KERNELEX(CryptGenRandom, BOOL, (HCRYPTPROV hProv, DWORD dwLen, BYTE* pbBuffer)) {
	auto RtlGenRandom = function_cast<BOOLEAN(__stdcall*)(PVOID RandomBuffer, ULONG RandomBufferLength)>(GetProcAddress(GetModuleHandle(LIBNAME), "SystemFunction036"));
	return RtlGenRandom && RtlGenRandom(pbBuffer, dwLen);
}

STUB_KERNELEX(CryptReleaseContext, BOOL, (HCRYPTPROV hProv, DWORD dwFlags)) {
	return TRUE;
}
#endif

STUB_WITH_LIBRARY(CryptEnumProvidersW, BOOL, (DWORD dwIndex, DWORD *pdwReserved, DWORD dwFlags, DWORD *pdwProvType, LPWSTR szProvName, DWORD *pcbProvName)) {
	SetLastError(ERROR_NO_MORE_ITEMS);
	return 0;
}

STUB_WITH_LIBRARY(IsWellKnownSid, BOOL, (PSID pSid, WELL_KNOWN_SID_TYPE WellKnownSidType)) {
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return 0;
}

STUB_WITH_LIBRARY(CheckTokenMembership, BOOL, (HANDLE TokenHandle, PSID SidToCheck, PBOOL IsMember)) {
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return 0;
}

#undef LIBNAME
#define LIBNAME __TEXT("shell32.dll")

STUB_WITH_LIBRARY(SHGetSpecialFolderPathW, BOOL, (HWND hwnd, LPSTR pszPath, int csidl, BOOL fCreate)) {
	return 0;
}

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

STUB_WITH_LIBRARY(ConvertFiberToThread, BOOL, ()) {
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

STUB_WITH_LIBRARY(GetModuleHandleExW, BOOL, (DWORD dwFlags, LPCWSTR lpModuleName, HMODULE* phModule)) {
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

STUB_WITH_LIBRARY(AddVectoredExceptionHandler, PVOID, (ULONG First, PVECTORED_EXCEPTION_HANDLER Handler)) {
	return NULL;
}

STUB_WITH_LIBRARY(RemoveVectoredExceptionHandler, ULONG, (PVOID Handle)) {
	return 0;
}

STUB_WITH_LIBRARY(AttachConsole, BOOL, (DWORD dwProcessId)) {
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return 0;
}

STUB_WITH_LIBRARY(VerSetConditionMask, ULONGLONG, (ULONGLONG dwlConditionMask, DWORD dwTypeBitMask, BYTE dwConditionMask)) {
	if (dwTypeBitMask == 0)
		return dwlConditionMask;
	dwConditionMask &= 0x07;
	if (dwConditionMask == 0)
		return dwlConditionMask;

	if (dwTypeBitMask & VER_PRODUCT_TYPE)
		dwlConditionMask |= dwConditionMask << 7*3;
	else if (dwTypeBitMask & VER_SUITENAME)
		dwlConditionMask |= dwConditionMask << 6*3;
	else if (dwTypeBitMask & VER_SERVICEPACKMAJOR)
		dwlConditionMask |= dwConditionMask << 5*3;
	else if (dwTypeBitMask & VER_SERVICEPACKMINOR)
		dwlConditionMask |= dwConditionMask << 4*3;
	else if (dwTypeBitMask & VER_PLATFORMID)
		dwlConditionMask |= dwConditionMask << 3*3;
	else if (dwTypeBitMask & VER_BUILDNUMBER)
		dwlConditionMask |= dwConditionMask << 2*3;
	else if (dwTypeBitMask & VER_MAJORVERSION)
		dwlConditionMask |= dwConditionMask << 1*3;
	else if (dwTypeBitMask & VER_MINORVERSION)
		dwlConditionMask |= dwConditionMask << 0*3;
	return dwlConditionMask;
}

STUB_WITH_LIBRARY(VerifyVersionInfoA, BOOL, (LPOSVERSIONINFOEXA lpVersionInfo, DWORD dwTypeMask, DWORDLONG dwlConditionMask)) {
	return 0;
}

STUB_WITH_LIBRARY(GetLongPathNameW, DWORD, (LPCWSTR lpszShortPath, LPWSTR lpszLongPath, DWORD cchBuffer)) {
	auto len = wcslen(lpszShortPath);
	if(len>=cchBuffer)
		return len + 1;
	wcscpy(lpszLongPath, lpszShortPath);
	return len;
}

#endif

#undef LIBNAME
#define LIBNAME __TEXT("secur32.dll")

STUB_WITH_LIBRARY(AcquireCredentialsHandleW, SECURITY_STATUS,
	(SEC_CHAR *pszPrincipal, SEC_CHAR *pszPackage, ULONG fCredentialUse, PLUID pvLogonID, PVOID pAuthData,
	SEC_GET_KEY_FN pGetKeyFn, PVOID pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)) {
	return SEC_E_INTERNAL_ERROR;
}

STUB_WITH_LIBRARY(CompleteAuthToken, SECURITY_STATUS, (PCtxtHandle phContext, PSecBufferDesc pToken)) {
	return SEC_E_INTERNAL_ERROR;
}

STUB_WITH_LIBRARY(InitializeSecurityContextW, SECURITY_STATUS,
	(PCredHandle phCredential, PCtxtHandle phContext, PSECURITY_STRING pTargetName, unsigned long fContextReq,
	unsigned long Reserved1, unsigned long TargetDataRep, PSecBufferDesc pInput,
	unsigned long Reserved2, PCtxtHandle phNewContext, PSecBufferDesc pOutput,
	unsigned long *pfContextAttr, PTimeStamp ptsExpiry)) {
	return SEC_E_INTERNAL_ERROR;
}

STUB_WITH_LIBRARY(QuerySecurityPackageInfoW, SECURITY_STATUS, (PSECURITY_STRING pPackageName, PSecPkgInfoW *ppPackageInfo)) {
	return SEC_E_INTERNAL_ERROR;
}

STUB_WITH_LIBRARY(DeleteSecurityContext, SECURITY_STATUS, (PCtxtHandle phContext)) {
	return SEC_E_INVALID_HANDLE;
}

STUB_WITH_LIBRARY(FreeContextBuffer, SECURITY_STATUS, (PVOID pvContextBuffer)) {
	return SEC_E_INVALID_HANDLE;
}

STUB_WITH_LIBRARY(FreeCredentialsHandle, SECURITY_STATUS, (PCredHandle phCredential)) {
	return SEC_E_INVALID_HANDLE;
}

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
	return count;
}

ULONG __stdcall internalimplif_nametoindex(PCSTR * InterfaceName) {
	return 0;
}

STUB_WITH_CALLABLE(if_nametoindex, internalimplif_nametoindex)

extern "C" ULONG __stdcall if_nametoindex(PCSTR* InterfaceName) {
	return 0;
}

#undef LIBNAME
#ifndef _WIN64
#undef CALLING_CONVENTION
#define CALLING_CONVENTION __cdecl

#define LIBNAME __TEXT("msvcrt.dll")
STUB_WITH_LIBRARY(_aligned_malloc, void*, (size_t size, size_t alignment)) {
	return __mingw_aligned_malloc(size, alignment);
}
STUB_WITH_LIBRARY(_aligned_free, void, (void* ptr)) {
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

STUB_WITH_LIBRARY(qsort_s, void, (void* base, size_t num, size_t width, qsortfunc compare, void* context)) {
	sorter.m_callback = compare;
	sorter.m_ptr = context;
	qsort(base, num, width, SortCallback);
}

STUB_WITH_LIBRARY(fopen_s, errno_t, (FILE** stream, char const* file, char const* mode)) {
	*stream = fopen(file, mode);
	return errno;
}
#endif

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
