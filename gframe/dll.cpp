#ifdef YGOPRO_BUILD_DLL
#include "dll.h"

#include "compiler_features.h"

#if EDOPRO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void DllDeleter::operator()(void* library) {
	FreeLibrary(reinterpret_cast<HMODULE>(library));
}

Dll Dll::OpenLibrary(epro::path_stringview path) {
	return Dll{ reinterpret_cast<void*>(LoadLibrary(path.data())) };
}

void* Dll::GetSymbol(epro::stringview sym) const {
	if(!get())
		return nullptr;
	return function_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(get()), sym.data()));
}

#elif EDOPRO_ANDROID

#include <dlfcn.h>
#include <fcntl.h> //open()
#include <unistd.h> //close()

#include "fmt.h"
#include "porting.h"
#include "utils.h"

struct AndroidLibrary {
	void* library;
	int fd;
};

void DllDeleter::operator()(void* library) {
	AndroidLibrary* alibrary = static_cast<AndroidLibrary*>(library);
	dlclose(alibrary->library);
	close(alibrary->fd);
	delete alibrary;
}

Dll Dll::OpenLibrary(epro::path_stringview path) {
	void* lib = nullptr;
	auto dest_path = epro::format("{}/{}XXXXXX.so", porting::internal_storage, ygo::Utils::GetFileName(path));
	auto output = mkstemps(&dest_path[0], 3);
	if(output == -1)
		return Dll{ nullptr };
	auto input = open(path.data(), O_RDONLY);
	if(input == -1) {
		unlink(dest_path.data());
		close(output);
		return Dll{ nullptr };
	}
	ygo::Utils::FileCopyFD(input, output);
	lib = dlopen(dest_path.data(), RTLD_NOW);
	unlink(dest_path.data());
	if(!lib) {
		close(output);
		close(input);
		return Dll{ nullptr };
	}
	close(input);
	auto library = new AndroidLibrary;
	library->library = lib;
	library->fd = output;
	return Dll{ reinterpret_cast<void*>(library) };
}

void* Dll::GetSymbol(epro::stringview sym) const {
	if(!get())
		return nullptr;
	return dlsym(static_cast<AndroidLibrary*>(get())->library, sym.data());
}

#else

#include <dlfcn.h>

void DllDeleter::operator()(void* library) {
	dlclose(library);
}

Dll Dll::OpenLibrary(epro::path_stringview path) {
	return Dll{ dlopen(path.data(), RTLD_NOW) };
}

void* Dll::GetSymbol(epro::stringview sym) const {
	if(!get())
		return nullptr;
	return dlsym(get(), sym.data());
}

#endif

#endif //YGOPRO_BUILD_DLL
