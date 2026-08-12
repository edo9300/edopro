#ifndef DLL_H
#define DLL_H
#include <memory>

#include "text_types.h"
#include "utils.h"

struct DllDeleter {
	void operator()(void* library);
};

class Dll : private std::unique_ptr<void, DllDeleter> {
	using unique_ptr::unique_ptr;
public:
	using unique_ptr::operator bool;
	static Dll OpenLibrary(epro::path_stringview path);

	void* GetSymbol(epro::stringview sym) const;

	template<typename T>
	T GetFunction(epro::stringview func) const {
		return function_cast<T>(GetSymbol(func));
	}
};

#endif /* DLL_H */
