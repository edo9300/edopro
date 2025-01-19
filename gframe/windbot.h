#ifndef WINDBOT_H
#define WINDBOT_H

#include <set>
#include <string>
#include <vector>
#include "config.h"
#if EDOPRO_LINUX || EDOPRO_MACOS
#include <sys/types.h>
#endif
#if EDOPRO_WINDOWS || EDOPRO_MACOS || EDOPRO_LINUX
#include <nlohmann/json.hpp>
#endif
#include "text_types.h"

namespace ygo {

struct WindBot {
	std::wstring name;
	std::wstring deck;
	std::wstring deckfile;
	int difficulty;
	std::set<int> masterRules;

#if EDOPRO_WINDOWS || EDOPRO_ANDROID || EDOPRO_IOS || EDOPRO_PSVITA
	using launch_ret_t = bool;
#elif EDOPRO_MACOS || EDOPRO_LINUX
	using launch_ret_t = pid_t;
	static epro::path_string executablePath;
#endif
	launch_ret_t Launch(int port, epro::wstringview pass, bool chat, int hand, const wchar_t* overridedeck) const;
	std::wstring GetLaunchParameters(int port, epro::wstringview pass, bool chat, int hand, const wchar_t* overridedeck) const;

#if EDOPRO_WINDOWS || EDOPRO_MACOS || EDOPRO_LINUX
	static nlohmann::ordered_json databases;
	static bool serialized;
#if EDOPRO_WINDOWS
	static std::wstring serialized_databases;
#else
	static std::string serialized_databases;
#endif
#endif

	static void AddDatabase(epro::path_stringview database);
};

}

#endif
