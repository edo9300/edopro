#include <IrrCompileConfig.h>
#include <path.h>
#include "game_config.h"
#include <fmt/format.h>
#include "bufferio.h"
#include "porting.h"
#include "utils.h"
#include "config.h"
#include "logging.h"
#include "file_stream.h"

namespace ygo {

GameConfig::GameConfig() {
#if EDOPRO_ANDROID
	Load(epro::format("{}/system.conf", porting::internal_storage));
#endif
	Load(EPRO_TEXT("./config/system.conf"));
	if(configs.empty()) {
		{
			FileStream conf_file{ EPRO_TEXT("./config/configs.json"), FileStream::in };
			if(!conf_file.fail()) {
				try {
					conf_file >> configs;
				}
				catch(const std::exception& e) {
					ErrorLog("Exception occurred while loading configs.json: {}", e.what());
					//throw(e);
				}
			}
		}
		{
			FileStream user_conf_file{ EPRO_TEXT("./config/user_configs.json"), FileStream::in };
			if(!user_conf_file.fail()) {
				try {
					user_conf_file >> user_configs;
				}
				catch(const std::exception& e) {
					ErrorLog("Exception occurred while loading user_configs.json: {}", e.what());
					//throw(e);
				}
			}
		}
	}
}

template<typename T, typename tag = T>
T parseOption(std::string& value) {
	if(std::is_unsigned<T>::value) {
		if(std::is_same<T, uint64_t>::value)
			return static_cast<T>(std::stoull(value));
		return static_cast<T>(std::stoul(value));
	}
	if(std::is_same<T, int64_t>::value)
		return static_cast<T>(std::stoll(value));
	return static_cast<T>(std::stoi(value));
}

template<>
bool parseOption<bool>(std::string& value) {
	return !!std::stoi(value);
}

template<>
float parseOption<float>(std::string& value) {
	return std::stof(value);
}

template<>
std::string parseOption(std::string& value) {
	return value;
}

template<>
std::wstring parseOption(std::string& value) {
	return BufferIO::DecodeUTF8(value);
}

template<>
irr::video::E_DRIVER_TYPE parseOption<irr::video::E_DRIVER_TYPE>(std::string& value) {
	Utils::ToUpperNoAccentsSelf(value);
#if !EDOPRO_ANDROID
	if(value == "OPENGL")
		return irr::video::EDT_OPENGL;
#ifdef _WIN32
	if(value == "D3D9")
		return irr::video::EDT_DIRECT3D9;
#endif
#endif
#if !EDOPRO_MACOS && IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9
	if(value == "OGLES1")
		return irr::video::EDT_OGLES1;
	if(value == "OGLES2")
		return irr::video::EDT_OGLES2;
#endif
	return irr::video::EDT_COUNT;
}

template<>
ygo::GameConfig::TextFont parseOption<ygo::GameConfig::TextFont>(std::string& value) {
	ygo::GameConfig::TextFont ret;
	auto pos = value.find(' ');
	if(pos == std::string::npos) {
		ret.font = Utils::ToPathString(value);
		return ret;
	}
	ret.font = Utils::ToPathString(value.substr(0, pos));
	ret.size = static_cast<uint8_t>(std::stoi(value.substr(pos)));
	return ret;
}

template<>
ygo::GameConfig::FallbackFonts parseOption<ygo::GameConfig::FallbackFonts>(std::string& value) {
	ygo::GameConfig::FallbackFonts ret;
#ifdef YGOPRO_USE_BUNDLED_FONT
	bool listed_bundled = false;
#endif
	for(auto& font : Utils::TokenizeString(value, '"')) {
		if(font.find_first_not_of(' ') == std::string::npos)
			continue;
		const auto parsed_font = parseOption<GameConfig::TextFont>(font);
		if(parsed_font.font == EPRO_TEXT("bundled"))
#ifdef YGOPRO_USE_BUNDLED_FONT
			listed_bundled = true;
#else
			continue;
#endif
		ret.push_back(std::move(parsed_font));
	}

#ifdef YGOPRO_USE_BUNDLED_FONT
	if(!listed_bundled)
		ret.emplace_back(ygo::GameConfig::TextFont{ { EPRO_TEXT("bundled") }, 12 });
#endif
	return ret;
}

template<>
int parseOption<int, ygo::GameConfig::MaxFPSConfig>(std::string& value) {
	int val = static_cast<int32_t>(std::stol(value));
	if(val < 0 && val != -1)
		val = static_cast<uint32_t>(std::stoul(value));
	return val;
}

template<>
int parseOption<int, ygo::GameConfig::MusicConfig>(std::string& value) {
	return std::min(std::max(std::stoi(value), 0), 100);;
}

template<>
uint8_t parseOption<uint8_t, ygo::GameConfig::BoolAsInt>(std::string& value) {
	return !!std::stoi(value);
}

template<typename T>
std::string serializeOption(const T& value) {
	return fmt::to_string(value);
}

template<>
std::string serializeOption(const uint8_t& value) {
	return fmt::to_string((int)value);
}

template<>
std::string serializeOption(const float& value) {
	return std::to_string(value);
}

template<>
std::string serializeOption(const bool& value) {
	return fmt::to_string((int)value);
}

template<>
std::string serializeOption<std::wstring>(const std::wstring& value) {
	return BufferIO::EncodeUTF8(value);
}

template<>
std::string serializeOption(const ygo::GameConfig::TextFont& value) {
	return epro::format("{} {}", Utils::ToUTF8IfNeeded(value.font), value.size);
}

template<>
std::string serializeOption(const ygo::GameConfig::FallbackFonts& value) {
	std::string res;
	for(const auto& font : value) {
		res.append(1, '"').append(serializeOption(font)).append("\" ");
	}
	if(!res.empty())
		res.pop_back();
	return res;
}

template<>
std::string serializeOption(const irr::video::E_DRIVER_TYPE& driver) {
	switch(driver) {
	case irr::video::EDT_OPENGL:
		return "opengl";
	case irr::video::EDT_DIRECT3D9:
		return "d3d9";
#if (IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9)
	case irr::video::EDT_OGLES1:
		return "ogles1";
	case irr::video::EDT_OGLES2:
		return "ogles2";
#endif
	case irr::video::EDT_COUNT:
		return "default";
	default:
		return "";
	}
}

bool GameConfig::Load(const epro::path_stringview filename) {
	FileStream conf_file{ filename.data(), FileStream::in };
	if(conf_file.fail())
		return false;
	std::string str;
	while (std::getline(conf_file, str)) {
		auto pos = str.find('\r');
		if(str.size() && pos != std::string::npos)
			str.erase(pos);
		if (str.empty() || str.at(0) == '#') {
			continue;
		}
		pos = str.find('=');
		if (pos == std::string::npos)
			continue;
		auto type = str.substr(0, pos - 1);
		str.erase(0, pos + 2);
		try {
#define OPTION_ALIASED_TAGGED(_type, tag, name, alias, ...) if(type == #alias){name=parseOption<_type,tag>(str); continue;}
#include "game_config.inl"
#undef OPTION_ALIASED_TAGGED
		}
		catch (...) {}
	}
	return true;
}

bool GameConfig::Save(const epro::path_stringview filename) {
	FileStream conf_file{ filename.data(), FileStream::out };
	if(conf_file.fail())
		return false;
	conf_file << "# Project Ignis: EDOPro system.conf\n";
	conf_file << "# Overwritten on normal game exit\n";
#define OPTION_ALIASED_TAGGED(_type, tag, name, alias, ...) conf_file << #alias " = " << serializeOption(name) << "\n";
#include "game_config.inl"
#undef OPTION_ALIASED_TAGGED
	return true;
}

}
