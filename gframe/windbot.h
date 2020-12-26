#ifndef WINDBOT_H
#define WINDBOT_H

#include <set>
#include <string>
#include <vector>
#if !defined(_WIN32) && !defined(__ANDROID__)
#include <sys/types.h>
#endif
#include "text_types.h"

namespace ygo {

struct WindBot {
	std::wstring name;
	std::wstring deck;
	/////kdiy//////
	std::wstring dialog;
	std::wstring deckfolder;
	std::wstring deckpath;
	/////kdiy//////	
	int difficulty;
	std::set<int> masterRules;

#if defined(_WIN32) || defined(__ANDROID__)
	bool Launch(int port, const std::wstring& pass, bool chat = true, int hand = 0) const;
#else
	pid_t Launch(int port, const std::wstring& pass, bool chat = true, int hand = 0) const;
#endif

	static uint32_t version;
	static epro::path_string executablePath;
};

}

#endif
