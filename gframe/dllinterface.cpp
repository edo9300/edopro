#include "dllinterface.h"

#include "compiler_features.h"
#include "dll.h"
#include "fmt.h"

#define X(type,name,...) extern "C" type name(__VA_ARGS__);
#include "ocgcore_functions.inl"

#ifdef YGOPRO_BUILD_DLL

#if EDOPRO_WINDOWS
#define CORENAME EPRO_TEXT("ocgcore.dll")
#elif EDOPRO_MACOS
#define CORENAME EPRO_TEXT("libocgcore.dylib")
#elif EDOPRO_IOS
#define CORENAME EPRO_TEXT("libocgcore-ios.dylib")
#elif EDOPRO_ANDROID
#if defined(__arm__)
#define CORENAME EPRO_TEXT("libocgcorev7.so")
#elif defined(__i386__)
#define CORENAME EPRO_TEXT("libocgcorex86.so")
#elif defined(__aarch64__)
#define CORENAME EPRO_TEXT("libocgcorev8.so")
#elif defined(__x86_64__)
#define CORENAME EPRO_TEXT("libocgcorex64.so")
#endif //__arm__
#elif EDOPRO_LINUX
#if defined(__aarch64__)
#define CORENAME EPRO_TEXT("libocgcore.aarch64.so")
#else
#define CORENAME EPRO_TEXT("libocgcore.so")
#endif
#elif EDOPRO_HAIKU
#define CORENAME EPRO_TEXT("libocgcore.haiku.so")
#endif //EDOPRO_WINDOWS

static epro::path_string GetCorePath(epro::path_stringview path) {
	return epro::format("{}" CORENAME, path);
}

bool Core::check_api_version() {
	OCG_GetVersion(&ver_major, &ver_minor);
	return (ver_major == EXPECTED_VERSION_MAJOR) && (ver_minor == EXPECTED_VERSION_MINOR);
}

#endif // YGOPRO_BUILD_DLL

std::shared_ptr<const Core> Core::LoadBundled() {
	auto core = std::shared_ptr<Core>(new Core{});
#define X(type,name,...) do{ core->name = ::name; } while(0);
#include "ocgcore_functions.inl"
	core->ver_major = OCG_VERSION_MAJOR;
	core->ver_minor = OCG_VERSION_MINOR;
	return core;
}

#ifdef YGOPRO_BUILD_DLL
std::shared_ptr<const Core> Core::Load(epro::path_stringview path) {
	if(path.empty()) {
		return nullptr;
	}
	auto core_path = GetCorePath(path);
	auto library = Dll::OpenLibrary(core_path);
	if(!library)
		return nullptr;
	auto core = std::shared_ptr<Core>(new Core{});
#define X(type,name,...) if((core->name = library.GetFunction<decltype(Core::name)>(#name)) == nullptr) return nullptr;
#include "ocgcore_functions.inl"
	if(!core->check_api_version())
		return nullptr;
	core->library = std::move(library);
	return core;
}
#endif

DuelPtr Core::CreateDuel(OCG_DuelOptions* options_ptr) const {
	OCG_Duel pduel{};
	auto payload = std::make_unique<Duel::ScriptReaderPayload>();
	payload->ogPayload = options_ptr->payload2;
	options_ptr->payload2 = payload.get();
	if(OCG_CreateDuel(&pduel, options_ptr) != OCG_DUEL_CREATION_SUCCESS)
		return nullptr;
	std::unique_ptr<Duel> duel{ new Duel{} };
	duel->core = this->shared_from_this();
	duel->thiz = pduel;
	payload->duel = duel.get();
	duel->scriptReaderPayload = std::move(payload);
	return duel;
}
