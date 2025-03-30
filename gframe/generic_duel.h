#ifndef GENERIC_DUEL_H
#define GENERIC_DUEL_H

#include <cstdint>
#include <set>
#include <type_traits>
#include "config.h"
#include "network.h"
#include "replay.h"
#include "deck_manager.h"

namespace ygo {

class GenericDuel final : public DuelMode {
public:
	GenericDuel(int team1 = 1, int team2 = 1, bool relay = false, int best_of = 0);
	~GenericDuel() override;
	void Chat(DuelPlayer* dp, void* pdata, int len) override;
	void JoinGame(DuelPlayer* dp, CTOS_JoinGame* pkt, bool is_creator) override;
	void LeaveGame(DuelPlayer* dp) override;
	void ToDuelist(DuelPlayer* dp) override;
	void ToObserver(DuelPlayer* dp) override;
	void PlayerReady(DuelPlayer* dp, bool ready) override;
	void PlayerKick(DuelPlayer* dp, uint8_t pos) override;
	void UpdateDeck(DuelPlayer* dp, void* pdata, uint32_t len) override;
	void StartDuel(DuelPlayer* dp) override;
	void HandResult(DuelPlayer* dp, uint8_t res) override;
	void RematchResult(DuelPlayer* dp, uint8_t rematch) override;
	void TPResult(DuelPlayer* dp, uint8_t tp) override;
	void Process() override;
	void Surrender(DuelPlayer* dp) override;
	int Analyze(CoreUtils::Packet packet) override;
	void GetResponse(DuelPlayer* dp, void* pdata, uint32_t len) override;
	void TimeConfirm(DuelPlayer* dp) override;
	void EndDuel() override;

	void BeforeParsing(const CoreUtils::Packet& packet, int& return_value, bool& record, bool& record_last);
	void Sending(CoreUtils::Packet& packet, int& return_value, bool& record, bool& record_last);
	void AfterParsing(const CoreUtils::Packet& packet, int& return_value, bool& record, bool& record_last);
	void DuelEndProc();
	void WaitforResponse(uint8_t player);
	void RefreshMzone(uint8_t player, uint32_t flag = 0x3881fff);
	void RefreshSzone(uint8_t player, uint32_t flag = 0x3e81fff);
	void RefreshHand(uint8_t player, uint32_t flag = 0x3781fff);
	void RefreshGrave(uint8_t player, uint32_t flag = 0x381fff);
	void RefreshExtra(uint8_t player, uint32_t flag = 0x381fff);
	void RefreshLocation(uint8_t player, uint32_t flag, uint8_t location);
	void RefreshSingle(uint8_t player, uint8_t location, uint8_t sequence, uint32_t flag = 0x3f81fff);

	static void GenericTimer(evutil_socket_t fd, short events, void* arg);

	void PseudoRefreshDeck(uint8_t player, uint32_t flag = 0x1181fff);
	static ReplayStream replay_stream;

protected:
	std::vector<CoreUtils::Packet> packets_cache;
	class duelist {
	public:
		DuelPlayer* player;
		bool ready;
		Deck odeck{};
		Deck pdeck{};
		uint32_t deck_error;
		duelist() : player(0), ready(false), deck_error(0) {}
		duelist(DuelPlayer* _player) : player(_player), ready(false), deck_error(0) {}
		void Clear() { player = nullptr; ready = false; pdeck.clear(); odeck.clear(); deck_error = 0; }
		operator bool() const {
			return !!player;
		}
		operator DuelPlayer*() const {
			return player;
		}
		bool operator==(DuelPlayer* rhs) {
			return rhs == player;
		}
		auto operator=(DuelPlayer* rhs) {
			Clear();
			player = rhs;
			return *this;
		}
	};
	bool CheckReady();
	int8_t GetFirstFree(int8_t start = 0);
	void SetAtPos(DuelPlayer* dp, size_t pos);
	duelist& GetAtPos(uint8_t pos);
	void Catchup(DuelPlayer* dp);
	uint8_t GetPos(DuelPlayer* dp);
	void OrderPlayers(std::vector<duelist>& players, size_t offset = 0);
	template<typename T, typename T2>
	inline void Iter(T& list, const T2& func) {
		for(auto& dlr : list) {
			if(dlr)
				func(dlr);
		}
	}

#if (!defined(_LIBCPP_VERSION) || _LIBCPP_VERSION >= 7000) && \
	((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
	template<typename T, typename... Arg>
	using FunctionResult = std::invoke_result_t<T, Arg...>;
#else
	template<typename T, typename... Arg>
	using FunctionResult = std::result_of_t<T(Arg...)>;
#endif

	template<typename T, typename T2>
	using EnableIf = std::enable_if_t<std::is_same_v<FunctionResult<T, duelist&>, T2>, int>;
	template<typename T, EnableIf<T, void> = 0>
	inline void IteratePlayers(T func) {
		Iter(players.home, func);
		Iter(players.opposing, func);
	}
	template<typename T, EnableIf<T, bool> = 0>
	inline bool IteratePlayers(T func) {
		for(auto& dueler : players.home) {
			if(dueler && !func(dueler))
				return false;
		}
		for(auto& dueler : players.opposing) {
			if(dueler && !func(dueler))
				return false;
		}
		return true;
	}
	template<typename T>
	inline void IteratePlayersAndObs(T func) {
		IteratePlayers(func);
		Iter(observers, func);
	}
	inline void ResendToAll();
	inline void ResendToAll(DuelPlayer* except);
	struct {
		std::vector<duelist> home;
		std::vector<duelist> opposing;
		uint32_t home_size, opposing_size;
		std::vector<duelist>::iterator home_iterator, opposing_iterator;
	} players;
	DuelPlayer* cur_player[2];
	std::set<DuelPlayer*> observers;
	uint8_t hand_result[2];
	uint8_t last_response;
	Replay last_replay;
	Replay new_replay;
	bool relay;
	int best_of;
	uint32_t match_kill;
	bool swapped;
	uint32_t turn_count;
	std::vector<uint8_t> match_result;
	uint16_t time_limit[2];
	int16_t grace_period;
};

}

#endif //GENERIC_DUEL_H

