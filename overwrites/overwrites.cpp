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
