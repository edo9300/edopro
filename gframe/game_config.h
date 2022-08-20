#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <nlohmann/json.hpp>
#include <EDriverTypes.h>
#include "config.h"
#include "text_types.h"
#include "config.h"

namespace ygo {

enum CoreLogOutput {
	CORE_LOG_NONE = 0x0,
	CORE_LOG_TO_CHAT = 0x1,
	CORE_LOG_TO_FILE = 0x2
};
#define OPTION_ALIASED(type, name, alias, ...) OPTION_ALIASED_TAGGED(type, type, name, alias, __VA_ARGS__)
#define OPTION_TAGGED(type, tag, name, ...) OPTION_ALIASED_TAGGED(type, tag, name, name, __VA_ARGS__)
#define OPTION(type, name, ...) OPTION_TAGGED(type, type, name, __VA_ARGS__)

struct GameConfig
{
	struct TextFont {
		epro::path_string font;
		uint8_t size;
	};
	struct MaxFPSConfig {};
	struct MusicConfig {};
	struct BoolAsInt {};
	GameConfig();
	bool Load(const epro::path_stringview filename);
	bool Save(const epro::path_stringview filename);
#define OPTION_ALIASED_TAGGED(type, tag, name, alias, ...) type name { __VA_ARGS__ };
#include "game_config.inl"
#undef OPTION_ALIASED_TAGGED
	std::wstring roompass; // NOT SERIALIZED
	std::string ssl_certificate_path;

	nlohmann::json configs;
	nlohmann::json user_configs;
};

extern GameConfig* gGameConfig;

}

#endif
