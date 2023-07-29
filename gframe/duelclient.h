#ifndef DUELCLIENT_H
#define DUELCLIENT_H

#include "config.h"
#include <vector>
#include <deque>
#include <set>
#include <atomic>
#include "epro_thread.h"
#include "epro_mutex.h"
#include "epro_condition_variable.h"
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include <fmt/format.h>
#include "network.h"
#include "data_manager.h"
#include "deck_manager.h"
#include "RNG/mt19937.h"
#include "replay.h"
#include "address.h"

namespace ygo {

class DuelClient {
private:
	static uint32_t connect_state;
	static std::vector<uint8_t> response_buf;
	static uint32_t watching;
	static uint8_t selftype;
	static bool is_host;
	static event_base* client_base;
	static bufferevent* client_bev;
	static bool is_closing;
	static uint64_t select_hint;
	static std::wstring event_string;
	static bool is_swapping;
	static bool stop_threads;
	static std::deque<std::vector<uint8_t>> to_analyze;
	static epro::mutex analyzeMutex;
	static epro::mutex to_analyze_mutex;
	static epro::thread parsing_thread;
	static epro::thread client_thread;
	static epro::condition_variable cv;
public:
	static RNG::mt19937 rnd;
	static epro::Address temp_ip;
	static uint16_t temp_port;
	static uint16_t temp_ver;
	static bool try_needed;
	static bool is_local_host;
	static std::atomic<bool> answered;

	static void JoinFromDiscord();
	static bool StartClient(const epro::Address& ip, uint16_t port, uint32_t gameid = 0, bool create_game = true);
	static void ConnectTimeout(evutil_socket_t fd, short events, void* arg);
	static void StopClient(bool is_exiting = false);
	static void ClientRead(bufferevent* bev, void* ctx);
	static void ClientEvent(bufferevent *bev, short events, void *ctx);
	static void ClientThread();
	static void HandleSTOCPacketLanSync(std::vector<uint8_t>&& data);
	static void HandleSTOCPacketLanAsync(const std::vector<uint8_t>& data);
	static void ParserThread();
	static bool CheckReady();
	static std::pair<uint32_t, uint32_t> GetPlayersCount();
	static ReplayStream replay_stream;
	static Replay last_replay;
	static int ClientAnalyze(const uint8_t* msg, uint32_t len);
	static int ClientAnalyze(const CoreUtils::Packet& packet) {
		return ClientAnalyze(packet.data(), static_cast<uint32_t>(packet.buff_size()));
	}
	static int GetSpectatorsCount() {
		return watching;
	};
	static void SwapField();
	static bool IsConnected() {
		return !!connect_state;
	};
	static void SetResponseB(const void* respB, size_t len) {
		response_buf.resize(len);
		memcpy(response_buf.data(), respB, len);
	}
	template<typename T>
	static inline void SetResponse(const T& resp) {
		return SetResponseB(&resp, sizeof(T));
	}
	static inline void SetResponseI(int respI) {
		return SetResponse<int32_t>(respI);
	}
	static void SendResponse();
	static void SendPacketToServer(uint8_t proto) {
		if(!client_bev)
			return;
		const auto res = [proto] {
			const uint16_t message_size = sizeof(proto);
			std::array<uint8_t, sizeof(message_size) + message_size> res;
			memcpy(res.data(), &message_size, sizeof(message_size));
			res[2] = proto;
			return res;
		}();
		bufferevent_write(client_bev, res.data(), res.size());
	}
	template<typename ST>
	static void SendPacketToServer(uint8_t proto, const ST& st) {
		if(!client_bev)
			return;
		const auto res = [proto, &st] {
			static constexpr uint16_t message_size = sizeof(proto) + sizeof(st);
			std::array<uint8_t, sizeof(message_size) + message_size> res;
			memcpy(res.data(), &message_size, sizeof(message_size));
			res[2] = proto;
			memcpy(res.data() + 3, &st, sizeof(st));
			return res;
		}();
		bufferevent_write(client_bev, res.data(), res.size());
	}
	static void SendBufferToServer(uint8_t proto, void* buffer, size_t len) {
		if(!client_bev)
			return;
		const auto res = [proto, buffer, len] {
			const uint16_t message_size = static_cast<uint16_t>(1 + len);
			std::vector<uint8_t> res;
			res.resize(sizeof(message_size) + message_size);
			memcpy(res.data(), &message_size, sizeof(message_size));
			res[2] = proto;
			memcpy(res.data() + 3, buffer, len);
			return res;
		}();
		bufferevent_write(client_bev, res.data(), res.size());
	}

	static void ReplayPrompt(bool need_header = false);

protected:
	static bool is_refreshing;
	static int match_kill;
	static event* resp_event;
public:
	static std::vector<epro::Host> hosts;
	static void BeginRefreshHost();
	static int RefreshThread(event_base* broadev);
	static void BroadcastReply(evutil_socket_t fd, short events, void* arg);
};

}

#endif //DUELCLIENT_H
