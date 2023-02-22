#ifndef _WIN64
#include <WinSock2.h>
#endif
#include <Windows.h>
#include <cstdint>
#include <array>
#include <utility>

namespace {
namespace Detail {

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
		const auto loaded = reinterpret_cast<FARPROC>(GetProcAddress(GetModuleHandle(libraryName), #funcname)); \
		return loaded ? loaded : reinterpret_cast<FARPROC>(internalimpl##funcname); \
	} \
}; \
ret __stdcall internalimpl##funcname args

#define STUB_WITH_LIBRARY(funcname,ret,args) STUB_WITH_LIBRARY_INT(__COUNTER__, funcname,ret,args, LIBNAME)

struct Args {
	FARPROC*& targetSymbol;
	FARPROC(__stdcall* load)();
};

template<size_t... I>
constexpr auto make_overridden_functions_array(std::index_sequence<I...> seq) {
	return std::array<Args, seq.size()>{Args{ Detail::Callable<I>::reference_symbol, Detail::Callable<I>::load }...};
}

} // namespace Detail
} // namespace

#define GET_OVERRIDDEN_FUNCTIONS_ARRAY() \
	Detail::make_overridden_functions_array(std::make_index_sequence<__COUNTER__>());

#define KERNELEX 0
#define LIBGIT2_1_4 1

/*
creates 2 functions, the stub function prefixed by handledxxx that is then exported via asm,
and internalimplxxx that is the fallback function called if the function isn't loaded at runtime
on first call GetProcAddress is called, and if the function is found, then that will be called onwards
otherwise fall back to the internal implementation
*/

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
#include <winternl.h>
#undef freeaddrinfo
#undef getaddrinfo
#undef getnameinfo
#endif

namespace {
#ifndef _WIN64
//some implementations taken from https://sourceforge.net/projects/win2kxp/

#if 0
//can't use c runtime functions as the runtime might not have been loaded yet
void __stdcall ___write(const char* ch) {
	DWORD dwCount;
	static HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsoleA(hOut, ch, strlen(ch), &dwCount, nullptr);
}
void __stdcall ___write(const wchar_t* ch) {
	DWORD dwCount;
	static HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsoleW(hOut, ch, wcslen(ch), &dwCount, nullptr);
}
#endif


STUB_WITH_CALLABLE(freeaddrinfo, WspiapiLoad(2))
STUB_WITH_CALLABLE(getaddrinfo, WspiapiLoad(0))
STUB_WITH_CALLABLE(getnameinfo, WspiapiLoad(1))

#define LIBNAME __TEXT("Advapi32.dll")

STUB_WITH_LIBRARY(IsWellKnownSid, BOOL, (PSID pSid, WELL_KNOWN_SID_TYPE WellKnownSidType)) {
	return FALSE;
}
#if KERNELEX
const bool kernelex = GetModuleHandle(__TEXT("ntdll.dll")) == nullptr;
inline bool IsUnderKernelex() {
	//ntdll.dll is loaded automatically in every windows nt process, but it seems it isn't in windows 9x
	return kernelex;
}
CHAR* convert_from_wstring(const WCHAR* wstr) {
	if(wstr == nullptr)
		return nullptr;
	const int wstr_len = (int)wcslen(wstr);
	const int num_chars = WideCharToMultiByte(CP_UTF8, 0, wstr, wstr_len, nullptr, 0, nullptr, nullptr);
	CHAR* strTo = (CHAR*)malloc((num_chars + 1) * sizeof(CHAR));
	if(strTo) {
		WideCharToMultiByte(CP_UTF8, 0, wstr, wstr_len, strTo, num_chars, nullptr, nullptr);
		strTo[num_chars] = '\0';
	}
	return strTo;
}

#define STUB_WITH_LIBRARY_KERNELEX_INT(index,funcname,ret,args,libraryName) \
extern "C" FARPROC* funcname##symbol; \
ret __stdcall internalimpl##funcname args ; \
template<> \
struct Detail::Callable<index> { \
	static constexpr auto& reference_symbol{funcname##symbol}; \
	static auto __stdcall load() { \
		const auto loaded = reinterpret_cast<FARPROC>(GetProcAddress(GetModuleHandle(libraryName), #funcname)); \
		return (IsUnderKernelex() || !loaded) ? reinterpret_cast<FARPROC>(internalimpl##funcname) : loaded; \
	} \
}; \
ret __stdcall internalimpl##funcname args

#define STUB_WITH_LIBRARY_KERNELEX(funcname,ret,args) STUB_WITH_LIBRARY_KERNELEX_INT(__COUNTER__, funcname,ret,args, LIBNAME)

STUB_WITH_LIBRARY_KERNELEX(CryptAcquireContextW, BOOL, (HCRYPTPROV* phProv, LPCWSTR pszContainer, LPCWSTR pszProvider, DWORD dwProvType, DWORD dwFlags)) {
	return pszContainer == nullptr && pszProvider == nullptr;
}

STUB_WITH_LIBRARY_KERNELEX(CryptGenRandom, BOOL, (HCRYPTPROV hProv, DWORD dwLen, BYTE* pbBuffer)) {
	auto RtlGenRandom = (BOOLEAN(__stdcall*)(PVOID RandomBuffer, ULONG RandomBufferLength))GetProcAddress(GetModuleHandle(LIBNAME), "SystemFunction036");
	return RtlGenRandom && RtlGenRandom(pbBuffer, dwLen);
}

STUB_WITH_LIBRARY_KERNELEX(CryptReleaseContext, BOOL, (HCRYPTPROV hProv, DWORD dwFlags)) {
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

BYTE slist_lock[0x100];
void SListLock(PSLIST_HEADER ListHead) {
	DWORD index = (((DWORD)ListHead) >> MEMORY_ALLOCATION_ALIGNMENT) & 0xFF;

	__asm {
		mov edx, index
		mov cl, 1
		spin:	mov al, 0
				lock cmpxchg byte ptr[slist_lock + edx], cl
				jnz spin
	};
	return;
}

void SListUnlock(PSLIST_HEADER ListHead) {
	DWORD index = (((DWORD)ListHead) >> MEMORY_ALLOCATION_ALIGNMENT) & 0xFF;
	slist_lock[index] = 0;
}
template<typename T>
auto GetListSequence(const T& list)->decltype(list->Sequence)& {
	return list->Sequence;
}
template<typename T>
auto GetListSequence(const T& list)->decltype(list->CpuId)& {
	return list->CpuId;
}

//Note: ListHead->Next.N== first node
STUB_WITH_LIBRARY(InterlockedPopEntrySList, PSLIST_ENTRY, (PSLIST_HEADER ListHead)) {
	PSLIST_ENTRY ret;
	SListLock(ListHead);
	if(!ListHead->Next.Next) ret = nullptr;
	else {
		ret = ListHead->Next.Next;
		ListHead->Next.Next = ret->Next;
		ListHead->Depth--;
		GetListSequence(ListHead)++;
	}
	SListUnlock(ListHead);
	return ret;
}

STUB_WITH_LIBRARY(InterlockedPushEntrySList, PSLIST_ENTRY, (PSLIST_HEADER ListHead, PSLIST_ENTRY ListEntry)) {
	PSLIST_ENTRY ret;
	SListLock(ListHead);
	ret = ListHead->Next.Next;
	ListEntry->Next = ret;
	ListHead->Next.Next = ListEntry;
	ListHead->Depth++;
	GetListSequence(ListHead)++;
	SListUnlock(ListHead);
	return ret;
}

STUB_WITH_LIBRARY(InterlockedFlushSList, PSLIST_ENTRY, (PSLIST_HEADER ListHead)) {
	PSLIST_ENTRY ret;
	SListLock(ListHead);
	ret = ListHead->Next.Next;
	ListHead->Next.Next = nullptr;
	ListHead->Depth = 0;
	GetListSequence(ListHead)++;
	SListUnlock(ListHead);
	return ret;
}


STUB_WITH_LIBRARY(InitializeSListHead, void, (PSLIST_HEADER ListHead)) {
	SListLock(ListHead);
	ListHead->Next.Next = nullptr;
	ListHead->Depth = 0;
	GetListSequence(ListHead) = 0;
	SListUnlock(ListHead);
}

STUB_WITH_LIBRARY(QueryDepthSList, USHORT, (PSLIST_HEADER ListHead)) {
	PSLIST_ENTRY n;
	USHORT depth = 0;
	SListLock(ListHead);
	n = ListHead->Next.Next;
	while(n) {
		depth++;
		n = n->Next;
	}
	SListUnlock(ListHead);
	return depth;
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

#if LIBGIT2_1_4
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
using fpRtlNtStatusToDosError = ULONG(WINAPI*)(DWORD Status);
using fpNtQuerySystemInformation = NTSTATUS(WINAPI*)(ULONG SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);

auto pRtlNtStatusToDosError = (fpRtlNtStatusToDosError)GetProcAddress(GetModuleHandle(__TEXT("ntdll.dll")), "RtlNtStatusToDosError");
auto pNtQuerySystemInformation = (fpNtQuerySystemInformation)GetProcAddress(GetModuleHandle(__TEXT("ntdll.dll")), "NtQuerySystemInformation");

STUB_WITH_LIBRARY(GetSystemTimes, BOOL, (LPFILETIME lpIdleTime, LPFILETIME lpKernelTime, LPFILETIME lpUserTime)) {
	if(!pRtlNtStatusToDosError || !pNtQuerySystemInformation)
		return FALSE;

	//Need atleast one out parameter
	if(!lpIdleTime && !lpKernelTime && !lpUserTime)
		return FALSE;

	//Get number of processors
	SYSTEM_BASIC_INFORMATION SysBasicInfo;
	NTSTATUS status = pNtQuerySystemInformation(SystemBasicInformation, &SysBasicInfo, sizeof(SysBasicInfo), nullptr);
	if(NT_SUCCESS(status)) {
		const auto total_size = sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * SysBasicInfo.NumberOfProcessors;
		auto hHeap = GetProcessHeap();
		auto* SysProcPerfInfo = static_cast<SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION*>(HeapAlloc(hHeap, 0, total_size));

		if(!SysProcPerfInfo) return FALSE;

		//Get counters
		status = pNtQuerySystemInformation(SystemProcessorPerformanceInformation, SysProcPerfInfo, total_size, nullptr);

		if(NT_SUCCESS(status)) {
			LARGE_INTEGER it = { 0, 0 }, kt = { 0, 0 }, ut = { 0, 0 };
			for(int i = 0; i < SysBasicInfo.NumberOfProcessors; ++i) {
				it.QuadPart += SysProcPerfInfo[i].IdleTime.QuadPart;
				kt.QuadPart += SysProcPerfInfo[i].KernelTime.QuadPart;
				ut.QuadPart += SysProcPerfInfo[i].UserTime.QuadPart;
			}

			if(lpIdleTime) {
				lpIdleTime->dwLowDateTime = it.LowPart;
				lpIdleTime->dwHighDateTime = it.HighPart;
			}

			if(lpKernelTime) {
				lpKernelTime->dwLowDateTime = kt.LowPart;
				lpKernelTime->dwHighDateTime = kt.HighPart;
			}

			if(lpUserTime) {
				lpUserTime->dwLowDateTime = ut.LowPart;
				lpUserTime->dwHighDateTime = ut.HighPart;
			}

			HeapFree(hHeap, 0, SysProcPerfInfo);
			return TRUE;
		} else {
			HeapFree(hHeap, 0, SysProcPerfInfo);
		}
	}

	SetLastError(pRtlNtStatusToDosError(status));
	return FALSE;
}
#endif

STUB_WITH_LIBRARY(GetNumaHighestNodeNumber, BOOL, (PULONG HighestNodeNumber)) {
	*HighestNodeNumber = 0;
	return TRUE;
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

STUB_WITH_LIBRARY(GetModuleHandleExA, BOOL, (DWORD dwFlags, LPCSTR lpModuleName, HMODULE* phModule)) {
	LoaderLock(TRUE);
	if(dwFlags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)
		*phModule = GetModuleHandleFromPtr(lpModuleName);
	else
		*phModule = GetModuleHandleA(lpModuleName);

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
		const auto func = (RtlGetVersionPtr)GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "RtlGetVersion");
		if(func && func(lpVersionInfo) == 0x00000000) {
			if(lpVersionInfo->dwMajorVersion != 5 || lpVersionInfo->dwMinorVersion != 0)
				return true;
		}
		return !!((decltype(&GetVersionExW))GetProcAddress(GetModuleHandle(LIBNAME), "GetVersionExW"))(lpVersionInfo);
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

STUB_WITH_LIBRARY(GetLogicalProcessorInformation, BOOL, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION Buffer, PDWORD ReturnedLength)) {
	*ReturnedLength = 0;
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

STUB_WITH_LIBRARY(EncodePointer, PVOID, (PVOID ptr)) {
	return (PVOID)((UINT_PTR)ptr ^ 0xDEADBEEF);
}

STUB_WITH_LIBRARY(DecodePointer, PVOID, (PVOID ptr)) {
	return (PVOID)((UINT_PTR)ptr ^ 0xDEADBEEF);
}
#endif

#if LIBGIT2_1_4
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
#endif

ULONG __stdcall internalimplif_nametoindex(PCSTR * InterfaceName) {
	return 0;
}

STUB_WITH_CALLABLE(if_nametoindex, internalimplif_nametoindex)

extern "C" ULONG __stdcall if_nametoindex(PCSTR* InterfaceName) {
	return 0;
}

/*
* Manually replace the various __imp__ pointers with the right function
* (or the one loaded from a dll or the reimplementation)
* this relies on a symbol holding the function pointer to be exported
* from the asm file that will then be referenced in the functions array
*/
extern "C" void __stdcall LoadSymbols() {
	static constexpr auto functionArray = GET_OVERRIDDEN_FUNCTIONS_ARRAY();
	for(const auto& obj : functionArray)
		*obj.targetSymbol = obj.load();
}
}
