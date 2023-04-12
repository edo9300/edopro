#ifndef WINDBOT_H
#define WINDBOT_H

#include <set>
#include <string>
#include <vector>
#include "config.h"
#if !defined(_WIN32) && !EDOPRO_ANDROID
#include <sys/types.h>
#endif
#if !EDOPRO_ANDROID
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

#if defined(_WIN32) || EDOPRO_ANDROID
	using launch_ret_t = bool;
#else
	using launch_ret_t = pid_t;
	static epro::path_string executablePath;
#endif
	launch_ret_t Launch(int port, epro::wstringview pass, bool chat, int hand, const wchar_t* overridedeck) const;
	std::wstring GetLaunchParameters(int port, epro::wstringview pass, bool chat, int hand, const wchar_t* overridedeck) const;

#if !EDOPRO_ANDROID
	static nlohmann::ordered_json databases;
	static bool serialized;
#ifdef _WIN32
	static std::wstring serialized_databases;
#else
	static std::string serialized_databases;
#endif
#endif

	static void AddDatabase(epro::path_stringview database);
};

}

#endif
