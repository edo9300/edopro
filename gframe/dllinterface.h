#ifndef DLL_INTERFACE_H
#define DLL_INTERFACE_H
#include "ocgapi_types.h"

#include "dll.h"
#include "text_types.h"

class Duel;
using DuelPtr = std::unique_ptr<const Duel>;

class Core : public std::enable_shared_from_this<Core> {
	friend class Duel;
private:
#ifdef YGOPRO_BUILD_DLL
	Dll library;
#endif
	explicit Core() = default;

	bool check_api_version();

#define X(type,name,...) type(*name)(__VA_ARGS__);
#include "ocgcore_functions.inl"
public:

	static constexpr inline int EXPECTED_VERSION_MINOR = OCG_VERSION_MINOR;
	static constexpr inline int EXPECTED_VERSION_MAJOR = OCG_VERSION_MAJOR;

	int ver_minor{};
	int ver_major{};

	static std::shared_ptr<const Core> LoadBundled();
#ifdef YGOPRO_BUILD_DLL
	static std::shared_ptr<const Core> Load(epro::path_stringview path);
#endif
	DuelPtr CreateDuel(OCG_DuelOptions* options_ptr) const;
};

class Duel {
	friend class Core;
public:
	struct ScriptReaderPayload {
		void* ogPayload;
		const Duel* duel;
	};
	std::unique_ptr<ScriptReaderPayload> scriptReaderPayload;
private:
	std::shared_ptr<const Core> core;
	OCG_Duel thiz;
	explicit Duel() = default;
public:
	~Duel() {
		core->OCG_DestroyDuel(thiz);
	}
#define ONLY_DUEL_FUNCTIONS
#define PREFIX(func) func
#define X(type,name,...) template<typename... Args> decltype(auto) name(Args&&... args) const { return core->OCG_##name(thiz,std::forward<Args>(args)...); }
#include "ocgcore_functions.inl"
};

using CorePtr = std::shared_ptr<const Core>;

#endif /* DLL_INTERFACE_H */
