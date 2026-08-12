#ifdef YGOPRO_BUILD_DLL

#include "dllinterface.h"

#include "compiler_features.h"
#include "crypto.h"
#include "dll.h"
#include "fmt.h"

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

epro::path_string GetCorePath(epro::path_stringview path) {
	return epro::format("{}" CORENAME, path);
}

bool Core::check_api_version() const {
	int max = 0, min = 0;
	OCG_GetVersion(&max, &min);
	return (max == OCG_VERSION_MAJOR) && (min == OCG_VERSION_MINOR);
}

std::unique_ptr<const Core> Core::Load(epro::path_stringview path) {
	auto core_path = GetCorePath(path);
	auto library = Dll::OpenLibrary(core_path);
	if(!library)
		return nullptr;
	auto core = std::unique_ptr<Core>(new Core{});
#define X(type,name,...) if((core->name = library.GetFunction<decltype(Core::name)>(#name)) == nullptr) return nullptr;
#include "ocgcore_functions.inl"
	if(!core->check_api_version())
		return nullptr;
	core->library = std::move(library);
	return core;
}

#endif //YGOPRO_BUILD_DLL
