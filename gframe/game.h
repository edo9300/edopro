#ifndef GAME_H
#define GAME_H

#include "config.h"
#include "netserver.h"
#include "deck_manager.h"
#include <unordered_map>
#include <vector>
#include <list>

namespace ygo {

class Game {

public:
	int MainServerLoop(const std::string& corepath);
	void LoadExpansionDB();
	void AddDebugMsg(const std::string& msg);
	static int GetMasterRule(uint32 param, uint32 forbiddentypes, int* truerule = nullptr);
	void* SetupDuel(uint32 seed);
	std::vector<unsigned char> LoadScript(const std::string& script_name, int& slen);
	std::vector<unsigned char> PreLoadScript(void* pduel, const std::string& script_name);
	static byte* ScriptReader(const char* script_name, int* slen);
	static int MessageHandler(void* fduel, int type);
	void PopulateResourcesDirectories();
#ifdef _WIN32
	std::vector<std::wstring> script_dirs;
#else
	std::vector<std::string> script_dirs;
#endif
#ifdef YGOPRO_BUILD_DLL
	void* ocgcore;
#endif
};

extern Game* mainGame;

}

#define UEVENT_EXIT			0x1
#define UEVENT_TOWINDOW		0x2

#define COMMAND_ACTIVATE	0x0001
#define COMMAND_SUMMON		0x0002
#define COMMAND_SPSUMMON	0x0004
#define COMMAND_MSET		0x0008
#define COMMAND_SSET		0x0010
#define COMMAND_REPOS		0x0020
#define COMMAND_ATTACK		0x0040
#define COMMAND_LIST		0x0080
#define COMMAND_OPERATION	0x0100
#define COMMAND_RESET		0x0200

#define POSITION_HINT		0x8000

#define DEFAULT_DUEL_RULE		4
#endif // GAME_H
