#include "netserver.h"
#include "generic_duel.h"
#include <thread>

namespace ygo {
bool operator==(const ClientVersion& ver1, const ClientVersion& ver2) {
	return ver1.client.major == ver2.client.major && ver1.client.minor == ver2.client.minor && ver1.core.major == ver2.core.major && ver1.core.minor == ver2.core.minor;
}
bool operator!=(const ClientVersion& ver1, const ClientVersion& ver2) {
	return !(ver1 == ver2);
}


std::unordered_map<bufferevent*, DuelPlayer> NetServer::users;
event_base* NetServer::net_evbase = 0;
evconnlistener* NetServer::listener = 0;
DuelMode* NetServer::duel_mode = 0;
char NetServer::net_server_read[0x20000];
char NetServer::net_server_write[0x20000];
unsigned short NetServer::last_sent = 0;

void NetServer::StartServer() {
	net_evbase = event_base_new();
	if(!net_evbase)
		return;
	sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(0);
	listener = evconnlistener_new_bind(net_evbase, ServerAccept, NULL,
									   LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (sockaddr*)&sin, sizeof(sin));
	if(!listener) {
		event_base_free(net_evbase);
		net_evbase = 0;
		return;
	}
	evconnlistener_set_error_cb(listener, ServerAcceptError);
	evutil_socket_t fd = evconnlistener_get_fd(listener);
	socklen_t addrlen = sizeof(sockaddr);
	sockaddr_in addr;
	getsockname(fd, (sockaddr*)&addr, &addrlen);
	printf("%u\n", ntohs(addr.sin_port));
	fflush(stdout);
	ServerThread();
}
void NetServer::StopServer() {
	if(!net_evbase)
		return;
	if(duel_mode)
		duel_mode->EndDuel();
	timeval etv = { 0, 1 };
	event_base_loopexit(net_evbase, &etv);
}
void NetServer::StopListen() {
	evconnlistener_disable(listener);
}
void NetServer::ServerAccept(evconnlistener* listener, evutil_socket_t fd, sockaddr* address, int socklen, void* ctx) {
	bufferevent* bev = bufferevent_socket_new(net_evbase, fd, BEV_OPT_CLOSE_ON_FREE);
	DuelPlayer dp;
	dp.name[0] = 0;
	dp.type = 0xff;
	dp.bev = bev;
	users[bev] = dp;
	bufferevent_setcb(bev, ServerEchoRead, NULL, ServerEchoEvent, NULL);
	bufferevent_enable(bev, EV_READ);
}
void NetServer::ServerAcceptError(evconnlistener* listener, void* ctx) {
	event_base_loopexit(net_evbase, 0);
}
void NetServer::ServerEchoRead(bufferevent *bev, void *ctx) {
	evbuffer* input = bufferevent_get_input(bev);
	size_t len = evbuffer_get_length(input);
	unsigned short packet_len = 0;
	while(true) {
		if(len < 2)
			return;
		evbuffer_copyout(input, &packet_len, 2);
		if(len < (size_t)packet_len + 2)
			return;
		evbuffer_remove(input, net_server_read, packet_len + 2);
		if(packet_len)
			HandleCTOSPacket(&users[bev], &net_server_read[2], packet_len);
		len -= packet_len + 2;
	}
}
void NetServer::ServerEchoEvent(bufferevent* bev, short events, void* ctx) {
	if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
		DuelPlayer* dp = &users[bev];
		DuelMode* dm = dp->game;
		if(dm)
			dm->LeaveGame(dp);
		else DisconnectPlayer(dp);
	}
}
int NetServer::ServerThread() {
	event_base_dispatch(net_evbase);
	for(auto bit = users.begin(); bit != users.end(); ++bit) {
		bufferevent_disable(bit->first, EV_READ);
		bufferevent_free(bit->first);
	}
	users.clear();
	evconnlistener_free(listener);
	listener = 0;
	if(duel_mode) {
		event_free(duel_mode->etimer);
		delete duel_mode;
	}
	duel_mode = 0;
	event_base_free(net_evbase);
	net_evbase = 0;
	return 0;
}
void NetServer::DisconnectPlayer(DuelPlayer* dp) {
	auto bit = users.find(dp->bev);
	if(bit != users.end()) {
		bufferevent_flush(dp->bev, EV_WRITE, BEV_FLUSH);
		bufferevent_disable(dp->bev, EV_READ);
		bufferevent_free(dp->bev);
		users.erase(bit);
	}
}
void NetServer::HandleCTOSPacket(DuelPlayer* dp, char* data, unsigned int len) {
	char* pdata = data;
	unsigned char pktType = BufferIO::ReadUInt8(pdata);
	if((pktType != CTOS_SURRENDER) && (pktType != CTOS_CHAT) && (dp->state == 0xff || (dp->state && dp->state != pktType)))
		return;
	switch(pktType) {
	case CTOS_RESPONSE: {
		if(!dp->game || !duel_mode->pduel)
			return;
		duel_mode->GetResponse(dp, pdata, len - 1);
		break;
	}
	case CTOS_TIME_CONFIRM: {
		if(!dp->game || !duel_mode->pduel)
			return;
		duel_mode->TimeConfirm(dp);
		break;
	}
	case CTOS_CHAT: {
		if(!dp->game)
			return;
		duel_mode->Chat(dp, pdata, len - 1);
		break;
	}
	case CTOS_UPDATE_DECK: {
		if(!dp->game)
			return;
		duel_mode->UpdateDeck(dp, pdata, len - 1);
		break;
	}
	case CTOS_HAND_RESULT: {
		if(!dp->game)
			return;
		CTOS_HandResult* pkt = (CTOS_HandResult*)pdata;
		dp->game->HandResult(dp, pkt->res);
		break;
	}
	case CTOS_TP_RESULT: {
		if(!dp->game)
			return;
		CTOS_TPResult* pkt = (CTOS_TPResult*)pdata;
		dp->game->TPResult(dp, pkt->res);
		break;
	}
	case CTOS_PLAYER_INFO: {
		CTOS_PlayerInfo* pkt = (CTOS_PlayerInfo*)pdata;
		BufferIO::CopyWStr(pkt->name, dp->name, 20);
		break;
	}
	case CTOS_CREATE_GAME: {
		if(dp->game || duel_mode)
			return;
		CTOS_CreateGame* pkt = (CTOS_CreateGame*)pdata;
		if(pkt->info.handshake != SERVER_HANDSHAKE) {
			VersionError scem{ ClientVersion{EXPAND_VERSION(CLIENT_VERSION)} };
			NetServer::SendPacketToPlayer(dp, STOC_ERROR_MSG, scem);
			return;
		}
		duel_mode = new GenericDuel(pkt->info.team1, pkt->info.team2, !!(pkt->info.duel_flag & DUEL_RELAY), pkt->info.best_of);
		duel_mode->etimer = event_new(net_evbase, 0, EV_TIMEOUT | EV_PERSIST, GenericDuel::GenericTimer, duel_mode);
		unsigned int hash = 1;
		for(auto lfit = deckManager._lfList.begin(); lfit != deckManager._lfList.end(); ++lfit) {
			if(pkt->info.lflist == lfit->hash) {
				hash = pkt->info.lflist;
				break;
			}
		}
		if(hash == 1)
			pkt->info.lflist = deckManager._lfList[0].hash;
		duel_mode->host_info = pkt->info;
		BufferIO::CopyWStr(pkt->name, duel_mode->name, 20);
		BufferIO::CopyWStr(pkt->pass, duel_mode->pass, 20);
		duel_mode->JoinGame(dp, 0, true);
		break;
	}
	case CTOS_JOIN_GAME: {
		if(!duel_mode || ((len - 1) < sizeof(CTOS_JoinGame)))
			break;
		duel_mode->JoinGame(dp, pdata, false);
		break;
	}
	case CTOS_LEAVE_GAME: {
		if(!duel_mode)
			break;
		duel_mode->LeaveGame(dp);
		break;
	}
	case CTOS_SURRENDER: {
		if(!duel_mode)
			break;
		duel_mode->Surrender(dp);
		break;
	}
	case CTOS_HS_TODUELIST: {
		if(!duel_mode || duel_mode->pduel)
			break;
		duel_mode->ToDuelist(dp);
		break;
	}
	case CTOS_HS_TOOBSERVER: {
		if(!duel_mode || duel_mode->pduel)
			break;
		duel_mode->ToObserver(dp);
		break;
	}
	case CTOS_HS_READY:
	case CTOS_HS_NOTREADY: {
		if(!duel_mode || duel_mode->pduel)
			break;
		duel_mode->PlayerReady(dp, (CTOS_HS_NOTREADY - pktType) != 0);
		break;
	}
	case CTOS_HS_KICK: {
		if(!duel_mode || duel_mode->pduel)
			break;
		CTOS_Kick* pkt = (CTOS_Kick*)pdata;
		duel_mode->PlayerKick(dp, pkt->pos);
		break;
	}
	case CTOS_HS_START: {
		if(!duel_mode || duel_mode->pduel)
			break;
		duel_mode->StartDuel(dp);
		break;
	}
	case CTOS_REMATCH_RESPONSE:	{
		if(!dp->game || !duel_mode || !duel_mode->seeking_rematch)
			break;
		CTOS_RematchResponse* pkt = (CTOS_RematchResponse*)pdata;
		duel_mode->RematchResult(dp, pkt->rematch);
		break;
	}
	}
}

}
