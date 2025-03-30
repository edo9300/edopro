#ifdef YGOPRO_USE_IRRKLANG
#include "irrklang_dynamic_loader.h"
#include <irrKlang.h>
#include <stdexcept>
#include "../../config.h"
#if EDOPRO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#ifdef IRRKLANG_STATIC
#include "../ikpmp3/ikpMP3.h"
#endif

#define CREATE_DEVICE_MSVC "?createIrrKlangDevice@irrklang@@YAPAVISoundEngine@1@W4E_SOUND_OUTPUT_DRIVER@1@HPBD1@Z"
#define CREATE_DEVICE_GCC "_ZN8irrklang20createIrrKlangDeviceENS_21E_SOUND_OUTPUT_DRIVEREiPKcS2_"

KlangLoader::KlangLoader() {
#ifndef IRRKLANG_STATIC
#if EDOPRO_WINDOWS
	library = LoadLibrary(TEXT("./irrKlang.dll"));
#elif EDOPRO_MACOS
	library = dlopen("./libIrrKlang.dylib", RTLD_NOW);
#elif EDOPRO_LINUX
	library = dlopen("./libIrrKlang.so", RTLD_NOW);
#endif //EDOPRO_WINDOWS
	if(library == nullptr)
		throw std::runtime_error("Failed to load irrklang library");
#if EDOPRO_WINDOWS
#ifdef _MSC_VER
	createdevice = (CreateDevice)GetProcAddress((HMODULE)library, CREATE_DEVICE_MSVC);
#else
	createdevice = (CreateDevice)GetProcAddress((HMODULE)library, CREATE_DEVICE_GCC);
#endif //_MSC_VER
#else
	createdevice = (CreateDevice)dlsym(library, CREATE_DEVICE_GCC);
#endif //EDOPRO_WINDOWS
	if(createdevice == nullptr)
		throw std::runtime_error("Failed to load createIrrKlangDevice function");
#else
	createdevice = irrklang::createIrrKlangDevice;
#endif //IRRKLANG_STATIC
}

KlangLoader::~KlangLoader() {
	if(!library)
		return;
#if EDOPRO_WINDOWS
	FreeLibrary((HMODULE)library);
#else
	dlclose(library);
#endif
}

irrklang::ISoundEngine* KlangLoader::createIrrKlangDevice() {
	auto engine = createdevice(irrklang::ESOD_AUTO_DETECT, irrklang::ESEO_DEFAULT_OPTIONS, 0, IRR_KLANG_VERSION);
	if(!engine)
		return nullptr;
#ifdef IRRKLANG_STATIC
	ikpMP3Init(engine);
#endif
	return engine;
}

#endif //YGOPRO_USE_IRRKLANG
