#ifndef SINGLE_MODE_H
#define SINGLE_MODE_H

#include "epro_thread.h"
#include "dllinterface.h"
#include "replay.h"
#include "mysignal.h"
#include "core_utils.h"
#include "text_types.h"
#include "handtrap_ai.h"
#include <memory>
#include <vector>

namespace ygo {

// Opponent's opening hand size stored in a hand test recording (it can differ from the player's).
uint32_t GetHandTestOpponentHand(epro::stringview script_name, uint32_t default_hand);

class SingleMode {
private:
	static DuelPtr pduel;
	static bool is_closing;
	static bool is_continuing;
	static bool is_restarting;
	static epro::thread single_mode_thread;

public:
	struct DuelOptions {
		uint32_t startingLP{ 8000 };
		uint32_t startingDrawCount{ 5 };
		uint32_t drawCountPerTurn{ 1 };
		uint64_t duelFlags{ 0 };
		bool handTestNoOpponent{ true };
		std::string scriptName;
		// hand test: cards that must be in the opening hand (the rest is random)
		std::vector<uint32_t> startingHand;
		// hand test: built-in opponent that only holds hand traps
		rka::HandtrapAI::Mode aiMode{ rka::HandtrapAI::OFF };
		std::vector<uint32_t> opponentHand;
		// deck whose optimiser tags tell the smart opponent what matters
		std::string deckName;
		// continue from a recorded duel instead of starting a new one
		std::shared_ptr<Replay> resume;
		size_t resumeResponses{ static_cast<size_t>(-1) };
		int resumeTurn{ 0 };
		enum ReturnTo : uint8_t {
			RETURN_AUTO, // deck editor after a hand test, puzzle list otherwise
			RETURN_DECK_EDIT,
			RETURN_REPLAY_LIST,
		};
		ReturnTo returnTo{ RETURN_AUTO };
		DuelOptions() {};
		explicit DuelOptions(epro::stringview filename) : scriptName(filename.data(), filename.size()) {};
	};

	static bool StartPlay(DuelOptions&& duelOptions);
	static void StopPlay(bool is_exiting = false);
	static void Restart();
	// Save states: a state is the recording of the duel so far, replayed
	// instantly when it is loaded.
	static bool CanSaveState();
	static bool SaveState(std::wstring* saved_name = nullptr);
	static bool LoadState(const epro::path_string& file, DuelOptions::ReturnTo return_to = DuelOptions::RETURN_AUTO);
	static std::vector<epro::path_string> ListStates();
	static bool DeleteState(const epro::path_string& file);
	static epro::path_string GetStateFolder();
	static void SetResponse(void* resp, size_t len);
	static int SinglePlayThread(DuelOptions&& duelOptions);
	static bool SinglePlayAnalyze(CoreUtils::Packet& packet);

	static void SinglePlayRefresh(uint8_t player, uint8_t location, uint32_t flag = 0x2f81fff);
	static void SinglePlayRefresh(uint32_t flag = 0x2f81fff);
	static void SinglePlayRefreshSingle(uint8_t player, uint8_t location, uint8_t sequence, uint32_t flag = 0x2f81fff);
	static void SinglePlayReload();
	static Signal singleSignal;

protected:
	static Replay last_replay;
	static Replay new_replay;
	static ReplayStream replay_stream;
	static rka::HandtrapAI ai;
	static std::shared_ptr<Replay> pending_resume;
	static std::shared_ptr<Replay> active_resume;
	static size_t resume_limit;
	static size_t resume_fed;
	static int resume_turn;
	static int turns_seen;
	static bool catching_up;
	static bool waiting_for_player;
	static bool ai_answered_last;
	static size_t size_before_ai_answer;
	static CoreUtils::Packet last_selection;
	static std::string state_deck_name;
	static void EndCatchUp();
	static bool FeedRecordedResponse();
};

}

#endif //SINGLE_MODE_H
