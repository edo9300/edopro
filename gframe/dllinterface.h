#ifndef DLL_INTERFACE_H
#define DLL_INTERFACE_H
#include "ocgapi_types.h"

#ifndef YGOPRO_BUILD_DLL
#define X(type,name,...) extern "C" type name(__VA_ARGS__);
#include "ocgcore_functions.inl"
#else
#include "text_types.h"

void UnloadCore(void* handle);
void* LoadOCGcore(epro::path_stringview path);
void* ChangeOCGcore(epro::path_stringview path, void* handle);

#define X(type,name,...) inline type(*name)(__VA_ARGS__);
#include "ocgcore_functions.inl"

#endif //YGOPRO_BUILD_DLL

#endif /* DLL_INTERFACE_H */
