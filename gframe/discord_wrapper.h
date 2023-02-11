#ifndef DISCORD_WRAPPER_H
#define DISCORD_WRAPPER_H

#include <string>
#include <cstdint>
#include "address.h"

class DiscordWrapper {
public:
	friend struct DiscordCallbacks;
	struct DiscordSecret {
		uint32_t game_id;
		epro::Address server_address;
		uint16_t server_port;
		std::wstring pass;
	};
	enum PresenceType {
		MENU,
		IN_LOBBY,
		DUEL,
		DUEL_STARTED,
		REPLAY,
		PUZZLE,
		HAND_TEST,
		DECK,
		DECK_SIDING,
		CLEAR,
		INITIALIZE,
		DISCONNECT,
		TERMINATE
	};
	bool Initialize();
	bool IsInitialized() const { return initialized; }
	bool IsConnected() const { return connected; }
	void UpdatePresence(PresenceType type);
	void Check();
private:
	uint32_t previous_gameid{ 0 };
	bool running{ false };
	bool initialized{ false };
	bool connected{ false };
	char secret_buf[128];
	PresenceType presence{ CLEAR };
	static void Connect();
	static void Disconnect();
};

#endif //DISCORD_WRAPPER_H
