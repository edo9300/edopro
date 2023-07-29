#ifndef SERVERLOBBY_H
#define SERVERLOBBY_H

#include "address.h"
#include "config.h"
#include "network.h"
#include <atomic>

namespace ygo {
struct ServerInfo {
	enum Protocol : uint8_t {
		HTTP,
		HTTPS,
	} protocol{ HTTP };
	mutable bool resolved{ false };
	std::wstring name;
	std::string address;
	std::string roomaddress;
	uint16_t duelport;
	uint16_t roomlistport;
	mutable epro::Host resolved_address;
	const epro::Host& Resolved() const;
	epro::stringview GetProtocolString() const {
		return GetProtocolString(protocol);
	}
	static epro::stringview GetProtocolString(Protocol protocol) {
		switch(protocol) {
		case HTTP:
			return "http";
		case HTTPS:
			return "https";
		default:
			unreachable();
		}
	}
	static Protocol GetProtocol(epro::stringview protocol) {
		if(protocol == "https")
			return HTTPS;
		return HTTP;
	}
};
struct RoomInfo {
	std::wstring name;
	uint32_t id;
	bool started;
	bool locked;
	std::vector<std::wstring> players;
	std::wstring description;
	HostInfo info;
};
class ServerLobby {
public:
	static std::vector<RoomInfo> roomsVector;
	static std::vector<ServerInfo> serversVector;
	static bool IsKnownHost(epro::Host host);
	static void RefreshRooms();
	static bool HasRefreshedRooms();
	static void GetRoomsThread();
	static void FillOnlineRooms();
	static void JoinServer(bool host);
	static std::atomic_bool is_refreshing;
	static std::atomic_bool has_refreshed;
};
//extern ServerLobby serverLobby;
}
#endif //SERVERLOBBY_H
