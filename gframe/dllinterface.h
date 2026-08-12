#ifndef DLL_INTERFACE_H
#define DLL_INTERFACE_H
#include "ocgapi_types.h"

#ifndef YGOPRO_BUILD_DLL
#define X(type,name,...) extern "C" type name(__VA_ARGS__);
#include "ocgcore_functions.inl"
#else
#include "dll.h"
#include "text_types.h"

class Core {
private:
	Dll library;
	explicit Core() = default;

	bool check_api_version() const;
public:
#define X(type,name,...) type(*name)(__VA_ARGS__);
#include "ocgcore_functions.inl"
	static std::unique_ptr<const Core> Load(epro::path_stringview path);
};

using CorePtr = std::unique_ptr<const Core>;

#endif //YGOPRO_BUILD_DLL

#endif /* DLL_INTERFACE_H */
