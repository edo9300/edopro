#include <IrrCompileConfig.h>
#include "game_config.h"
#include <fmt/format.h>
#include "bufferio.h"
#include "utils.h"
#include "config.h"
#include "logging.h"
#include "file_stream.h"

namespace ygo {

GameConfig::GameConfig() {
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
		if(std::is_same<T, int64_t>::value)
			return static_cast<T>(std::stoull(value));
		return static_cast<T>(std::stoul(value));
	} else {
		if(std::is_same<T, int64_t>::value)
			return static_cast<T>(std::stoll(value));
		return static_cast<T>(std::stoi(value));
	}
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
#ifndef __ANDROID__
	if(value == "OPENGL")
		return irr::video::EDT_OPENGL;
#ifdef _WIN32
	if(value == "D3D9")
		return irr::video::EDT_DIRECT3D9;
#endif
#endif
#if !defined(EDOPRO_MACOS) && IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9
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
	if(pos == std::wstring::npos) {
		ret.font = Utils::ToPathString(value);
		return ret;
	}
	ret.font = Utils::ToPathString(value.substr(0, pos));
	ret.size = std::stoi(value.substr(pos));
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
	return fmt::format("{} {}", Utils::ToUTF8IfNeeded(value.font), value.size);
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
		if (pos == std::wstring::npos)
			continue;
		auto type = str.substr(0, pos - 1);
		str.erase(0, pos + 2);
		try {
			if(type == "antialias")
				antialias = std::stoi(str);
			else if(type == "use_d3d") {
#ifdef _WIN32
				driver_type = std::stoi(str) ? irr::video::EDT_COUNT : irr::video::EDT_OPENGL;
#elif defined(__ANDROID__)
				driver_type = std::stoi(str) ? irr::video::EDT_COUNT : irr::video::EDT_OGLES1;
#endif
			}
			else if(type == "driver_type") {
				auto new_type = getDriverType(str);
				if(new_type != irr::video::EDT_NULL)
					driver_type = new_type;
			} else if(type == "fullscreen")
				fullscreen = !!std::stoi(str);
			else if(type == "nickname")
				nickname = BufferIO::DecodeUTF8(str);
			else if(type == "gamename")
				gamename = BufferIO::DecodeUTF8(str);
			else if(type == "lastdeck")
				lastdeck = BufferIO::DecodeUTF8(str);
			else if(type == "lastExtraRules")
				lastExtraRules = std::stoi(str);
			else if(type == "lastDuelForbidden")
				lastDuelForbidden = std::stoi(str);
			else if(type == "maxFPS") {
				int val = static_cast<int32_t>(std::stol(str));
				if(val < 0 && val != -1)
					val = static_cast<uint32_t>(std::stoul(str));
				maxFPS = val;
			} else if(type == "lastDuelParam") {
				lastDuelParam = static_cast<uint64_t>(std::stoull(str));
			}
#define DESERIALIZE_UNSIGNED(name) \
			else if (type == #name) { \
				name = static_cast<uint32_t>(std::stoul(str)); \
			}
			DESERIALIZE_UNSIGNED(coreLogOutput)
			DESERIALIZE_UNSIGNED(lastlflist)
			DESERIALIZE_UNSIGNED(lastallowedcards)
			DESERIALIZE_UNSIGNED(timeLimit)
			DESERIALIZE_UNSIGNED(team1count)
			DESERIALIZE_UNSIGNED(team2count)
			DESERIALIZE_UNSIGNED(bestOf)
			DESERIALIZE_UNSIGNED(startLP)
			DESERIALIZE_UNSIGNED(startHand)
			DESERIALIZE_UNSIGNED(drawCount)
			DESERIALIZE_UNSIGNED(lastBot)
#define DESERIALIZE_BOOL(name) else if (type == #name) name = !!std::stoi(str);
			DESERIALIZE_BOOL(relayDuel)
			DESERIALIZE_BOOL(noCheckDeck)
			DESERIALIZE_BOOL(hideHandsInReplays)
			DESERIALIZE_BOOL(noShuffleDeck)
#if defined(__linux__) && !defined(__ANDROID__) && (IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9)
			DESERIALIZE_BOOL(useWayland)
#endif
			DESERIALIZE_BOOL(vsync)
			DESERIALIZE_BOOL(showScopeLabel)
			DESERIALIZE_BOOL(saveHandTest)
			DESERIALIZE_BOOL(discordIntegration)
			DESERIALIZE_BOOL(loopMusic)
			DESERIALIZE_BOOL(noClientUpdates)
			DESERIALIZE_BOOL(logDownloadErrors)
			DESERIALIZE_BOOL(alternative_phase_layout)
#ifdef WIN32
			DESERIALIZE_BOOL(showConsole)
#endif
#ifndef __linux__
			else if(type == "windowStruct")
				windowStruct = str;
#endif
			else if(type == "botThrowRock")
				botThrowRock = !!std::stoi(str);
			else if(type == "botMute")
				botMute = !!std::stoi(str);
			else if(type == "lastServer")
				lastServer = BufferIO::DecodeUTF8(str);
			else if(type == "textfont") {
				pos = str.find(L' ');
				if(pos == std::wstring::npos) {
					textfont = BufferIO::DecodeUTF8(str);
					continue;
				}
				textfont = BufferIO::DecodeUTF8(str.substr(0, pos));
				textfontsize = std::stoi(str.substr(pos));
			} else if(type == "numfont")
				numfont = BufferIO::DecodeUTF8(str);
			else if(type == "serverport")
				serverport = BufferIO::DecodeUTF8(str);
			else if(type == "lasthost")
				lasthost = BufferIO::DecodeUTF8(str);
			else if(type == "lastport")
				lastport = BufferIO::DecodeUTF8(str);
			else if(type == "roompass")
				roompass = BufferIO::DecodeUTF8(str);
			else if(type == "game_version") {
				uint16_t version = static_cast<uint16_t>(std::stoul(str));
				if(version) {
					PRO_VERSION = version;
					game_version = PRO_VERSION;
				}
			} else if(type == "automonsterpos")
				chkMAutoPos = !!std::stoi(str);
			else if(type == "autospellpos")
				chkSTAutoPos = !!std::stoi(str);
			else if(type == "randompos")
				chkRandomPos = !!std::stoi(str);
			else if(type == "autochain")
				chkAutoChain = !!std::stoi(str);
			else if(type == "waitchain")
				chkWaitChain = !!std::stoi(str);
			else if(type == "mute_opponent")
				chkIgnore1 = !!std::stoi(str);
			else if(type == "mute_spectators")
				chkIgnore2 = !!std::stoi(str);
			else if(type == "hide_setname")
				chkHideSetname = !!std::stoi(str);
			else if(type == "hide_hint_button")
				chkHideHintButton = !!std::stoi(str);
			else if(type == "draw_field_spell")
				draw_field_spell = !!std::stoi(str);
			else if(type == "quick_animation")
				quick_animation = !!std::stoi(str);
			else if(type == "show_unofficial")
				chkAnime = !!std::stoi(str);
			else if(type == "showFPS")
				showFPS = !!std::stoi(str);
			else if(type == "hidePasscodeScope")
				hidePasscodeScope = !!std::stoi(str);
			else if(type == "filterBot")
				filterBot = !!std::stoi(str);
			else if(type == "save_filter_settings")
				save_filter_settings = !!std::stoi(str);
			else if(type == "dpi_scale")
				dpi_scale = std::stof(str);
			else if(type == "skin")
				skin = Utils::ToPathString(str);
			else if(type == "override_ssl_certificate_path")
				override_ssl_certificate_path = str;
			else if(type == "language")
				locale = Utils::ToPathString(str);
			else if(type == "scale_background")
				scale_background = !!std::stoi(str);
#ifndef __ANDROID__
			else if(type == "dotted_lines")
				dotted_lines = !!std::stoi(str);
			else if(type == "accurate_bg_resize")
				accurate_bg_resize = !!std::stoi(str);
#endif
			else if(type == "ctrlClickIsRMB")
				ctrlClickIsRMB = !!std::stoi(str);
			else if(type == "enable_music")
				enablemusic = !!std::stoi(str);
			else if(type == "enable_sound")
				enablesound = !!std::stoi(str);
			else if(type == "music_volume")
				musicVolume = std::min(std::max(std::stoi(str), 0), 100);
			else if(type == "sound_volume")
				soundVolume = std::min(std::max(std::stoi(str), 0), 100);
#ifdef __ANDROID__
			else if(type == "native_keyboard")
				native_keyboard = !!std::stoi(str);
			else if(type == "native_mouse")
				native_mouse = !!std::stoi(str);
#endif
			else if(type == "controller_input")
				controller_input = !!std::stoi(str);
			else if(type == "filterAllowedCards")
				lastSearchAllowedCards = !!std::stoi(str);
			else if(type == "filterForbidden")
				lastSearchForbidden = !!std::stoi(str);
			else if(type == "filterTeam1Count")
				lastSearchTeam1Count = std::min(std::max(std::stoi(str), 0), 3);
			else if(type == "filterTeam2Count")
				lastSearchTeam2Count = std::min(std::max(std::stoi(str), 0), 3);
			else if(type == "filterBestOf")
				lastSearchBestOf = std::min(std::max(std::stoi(str), 0), 100);
			else if(type == "filterRelayDuel")
				lastSearchRelayDuel = !!std::stoi(str);
			else if(type == "filterLocked")
				lastSearchLocked = !!std::stoi(str);
			else if(type == "filterActive")
				lastSearchActive = !!std::stoi(str);
			else if(type == "filterString")
				lastSearchFilterString = Utils::ToPathString(str);
		}
		catch (...) {}
	}
	return true;
}
#undef DESERIALIZE_UNSIGNED
#undef DESERIALIZE_BOOL

template<typename T>
inline void Serialize(std::ostream& conf_file, const char* name, T value) {
	conf_file << name << " = " << value << "\n";
}
template<>
inline void Serialize(std::ostream& conf_file, const char* name, float value) {
	conf_file << name << " = " << fmt::to_string(value) << "\n"; // Forces float to show decimals
}
template<>
inline void Serialize(std::ostream& conf_file, const char* name, std::wstring value) {
	conf_file << name << " = " << BufferIO::EncodeUTF8(value) << "\n";
}

bool GameConfig::Save(const epro::path_char* filename) {
#if defined(__MINGW32__) && defined(UNICODE)
	auto fd = _wopen(filename, _O_WRONLY);
	if(fd == -1)
		return false;
	__gnu_cxx::stdio_filebuf<char> b(fd, std::ios::out);
	std::ostream conf_file(&b);
#else
	std::ofstream conf_file(filename);
#endif
	if(conf_file.fail())
		return false;
	conf_file << "# Project Ignis: EDOPro system.conf\n";
	conf_file << "# Overwritten on normal game exit\n";
#define SERIALIZE(name) Serialize(conf_file, #name, name)
	conf_file << "driver_type = " << getDriverName(driver_type) << "\n";
#if defined(__linux__) && !defined(__ANDROID__) && (IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9)
	if(useWayland <= 1)
		Serialize(conf_file, "useWayland", useWayland == 1);
#endif
	SERIALIZE(vsync);
	SERIALIZE(maxFPS);
	SERIALIZE(fullscreen);
	SERIALIZE(showConsole);
	SERIALIZE(windowStruct);
	conf_file << "antialias = "          << ((int)antialias) << "\n";
	SERIALIZE(coreLogOutput);
	conf_file << "nickname = "           << BufferIO::EncodeUTF8(nickname) << "\n";
	conf_file << "gamename = "           << BufferIO::EncodeUTF8(gamename) << "\n";
	conf_file << "lastdeck = "           << BufferIO::EncodeUTF8(lastdeck) << "\n";
	conf_file << "lastlflist = "         << lastlflist << "\n";
	conf_file << "lastallowedcards = "   << lastallowedcards << "\n";
	SERIALIZE(lastDuelParam);
	SERIALIZE(lastExtraRules);
	SERIALIZE(lastDuelForbidden);
	SERIALIZE(timeLimit);
	SERIALIZE(team1count);
	SERIALIZE(team2count);
	SERIALIZE(bestOf);
	SERIALIZE(startLP);
	SERIALIZE(startHand);
	SERIALIZE(drawCount);
	SERIALIZE(relayDuel);
	SERIALIZE(noShuffleDeck);
	SERIALIZE(noCheckDeck);
	SERIALIZE(hideHandsInReplays);
	conf_file << "textfont = "                 << BufferIO::EncodeUTF8(textfont) << " " << std::to_string(textfontsize) << "\n";
	conf_file << "numfont = "                  << BufferIO::EncodeUTF8(numfont) << "\n";
	conf_file << "serverport = "               << BufferIO::EncodeUTF8(serverport) << "\n";
	conf_file << "lasthost = "                 << BufferIO::EncodeUTF8(lasthost) << "\n";
	conf_file << "lastport = "                 << BufferIO::EncodeUTF8(lastport) << "\n";
	conf_file << "botThrowRock = "             << botThrowRock << "\n";
	conf_file << "botMute = "                  << botMute << "\n";
	SERIALIZE(lastBot);
	conf_file << "lastServer = "               << BufferIO::EncodeUTF8(lastServer) << "\n";
	conf_file << "game_version = "             << game_version << "\n";
	conf_file << "automonsterpos = "           << chkMAutoPos << "\n";
	conf_file << "autospellpos = "             << chkSTAutoPos << "\n";
	conf_file << "randompos = "                << chkRandomPos << "\n";
	conf_file << "autochain = "                << chkAutoChain << "\n";
	conf_file << "waitchain = "                << chkWaitChain << "\n";
	conf_file << "mute_opponent = "            << chkIgnore1 << "\n";
	conf_file << "mute_spectators = "          << chkIgnore2 << "\n";
	conf_file << "hide_setname = "             << chkHideSetname << "\n";
	conf_file << "hide_hint_button = "         << chkHideHintButton << "\n";
	conf_file << "draw_field_spell = "         << draw_field_spell << "\n";
	conf_file << "quick_animation = "          << quick_animation << "\n";
	SERIALIZE(alternative_phase_layout);
	conf_file << "showFPS = "                  << showFPS << "\n";
	conf_file << "hidePasscodeScope = "        << hidePasscodeScope << "\n";
	SERIALIZE(showScopeLabel);
	conf_file << "filterBot = "                << filterBot << "\n";
	conf_file << "save_filter_settings = "     << save_filter_settings << "\n";
	conf_file << "show_unofficial = "          << chkAnime << "\n";
	conf_file << "ctrlClickIsRMB = "           << ctrlClickIsRMB << "\n";
	conf_file << "dpi_scale = "                << std::to_string(dpi_scale) << "\n"; // Forces float to show decimals
	conf_file << "skin = "            	       << Utils::ToUTF8IfNeeded(skin) << "\n";
	conf_file << "override_ssl_certificate_path = "<< override_ssl_certificate_path << "\n";
	conf_file << "language = "                 << Utils::ToUTF8IfNeeded(locale) << "\n";
	conf_file << "scale_background = "         << scale_background << "\n";
	conf_file << "dotted_lines = "             << dotted_lines << "\n";
	conf_file << "controller_input = "         << controller_input << "\n";
	conf_file << "accurate_bg_resize = "       << accurate_bg_resize << "\n";
	conf_file << "enable_music = "             << enablemusic << "\n";
	conf_file << "enable_sound = "             << enablesound << "\n";
	conf_file << "music_volume = "             << musicVolume << "\n";
	conf_file << "sound_volume = "             << soundVolume << "\n";
	conf_file << "filterAllowedCards = "	   << lastSearchAllowedCards << "\n";
	conf_file << "filterForbidden = "          << lastSearchForbidden << "\n";
	conf_file << "filterTeam1Count = "         << lastSearchTeam1Count << "\n";
	conf_file << "filterTeam2Count = "         << lastSearchTeam2Count << "\n";
	conf_file << "filterBestOf = "             << lastSearchBestOf << "\n";
	conf_file << "filterRelayDuel = "          << lastSearchRelayDuel << "\n";
	conf_file << "filterLocked = "             << lastSearchLocked << "\n";
	conf_file << "filterActive = "             << lastSearchActive << "\n";
	conf_file << "filterString = "             << Utils::ToUTF8IfNeeded(lastSearchFilterString)  << "\n";
	SERIALIZE(loopMusic);
	SERIALIZE(saveHandTest);
	SERIALIZE(discordIntegration);
	SERIALIZE(noClientUpdates);
	SERIALIZE(logDownloadErrors);
#ifdef __ANDROID__
	conf_file << "native_keyboard = "    << native_keyboard << "\n";
	conf_file << "native_mouse = "       << native_mouse << "\n";
#endif
#undef SERIALIZE
	return true;
}

}
