#ifdef YGOPRO_BUILD_DLL
#include <string>
#include "dll.h"

#include "compiler_features.h"
#if EDOPRO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include "porting.h"
#include <dlfcn.h>
#endif
#include "dllinterface.h"
#include "utils.h"
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

class Core {
#define X(type,name,...) type(*int_##name)(__VA_ARGS__);
#include "ocgcore_functions.inl"

	Dll library{ nullptr };
	bool valid{ false };
	bool enabled{ false };

	bool check_api_version() const {
		int max = 0, min = 0;
		int_OCG_GetVersion(&max, &min);
		return (max == OCG_VERSION_MAJOR) && (min == OCG_VERSION_MINOR);
	}
public:

	Core(epro::path_stringview path) {
		if(!library)
			return;
#define X(type,name,...) if((int_##name = library.GetFunction<decltype(name)>(#name)) == nullptr) return;
#include "ocgcore_functions.inl"
		valid = check_api_version();
	}
	~Core() {
		Disable();
	}
	void Enable() {
#define X(type,name,...) name = int_##name;
#include "ocgcore_functions.inl"
		enabled = true;
	}
	void Disable() {
		if(enabled) {
#define X(type,name,...) name = nullptr;
#include "ocgcore_functions.inl"
			enabled = false;
		}
	}
	bool IsValid() const {
		return valid;
	}
};

void* LoadOCGcore(epro::path_stringview path) {
	Core* core = new Core(path);
	if(!core->IsValid()) {
		delete core;
		return nullptr;
	}
	core->Enable();
	return core;
}

void UnloadCore(void* handle) {
	if(!handle)
		return;
	delete static_cast<Core*>(handle);
}

void* ChangeOCGcore(epro::path_stringview path, void* handle) {
	Core* newcore = new Core(path);
	if(!newcore->IsValid())
		return nullptr;
	UnloadCore(handle);
	newcore->Enable();
	return newcore;
}

#endif //YGOPRO_BUILD_DLL
