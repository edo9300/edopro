#ifndef REPLAY_H
#define REPLAY_H

#include <memory> //std::unique_ptr
#include <ctime>
#include <vector>
#include <fstream>
#include "RNG/Xoshiro256.hpp"
#include "config.h"
#include "core_utils.h"
#include "text_types.h"

namespace ygo {

#define REPLAY_COMPRESSED	0x1
#define REPLAY_TAG			0x2
#define REPLAY_DECODED		0x4
#define REPLAY_SINGLE_MODE	0x8
#define REPLAY_LUA64		0x10
#define REPLAY_NEWREPLAY	0x20
#define REPLAY_HAND_TEST	0x40
#define REPLAY_DIRECT_SEED	0x80
#define REPLAY_64BIT_DUELFLAG	0x100
#define REPLAY_EXTENDED_HEADER	0x200

#define REPLAY_YRP1			0x31707279
#define REPLAY_YRPX			0x58707279

struct ReplayHeader {
	uint32_t id;
	uint32_t version;
	uint32_t flag;
	uint32_t timestamp; //Used as both seed and timestamp if used as base header
	uint32_t datasize;
	uint32_t hash;
	uint8_t props[8];
};

struct ExtendedReplayHeader {
	static constexpr uint64_t latest_header_version = 1;
	ReplayHeader base;
	uint64_t header_version;
	uint64_t seed[4];
	void SetSeed(const RNG::Xoshiro256StarStar::StateType& seed_) {
		seed[0] = seed_[0];
		seed[1] = seed_[1];
		seed[2] = seed_[2];
		seed[3] = seed_[3];
	}
	static ExtendedReplayHeader CreateDefaultHeader(uint32_t replay_id, uint32_t timestamp) {
		ExtendedReplayHeader header{};
		header.header_version = latest_header_version;
		auto& base = header.base;
		base.timestamp = timestamp;
		base.id = replay_id;
		base.version = CLIENT_VERSION;
		base.flag = REPLAY_LUA64 | REPLAY_NEWREPLAY | REPLAY_64BIT_DUELFLAG | REPLAY_EXTENDED_HEADER;
		return header;
	}
	static bool ParseReplayHeader(const void* data, uint32_t input_len, ExtendedReplayHeader& header, uint32_t* header_length);
};

using cardlist_type = std::vector<uint32_t>;

struct ReplayDeck {
	cardlist_type main_deck, extra_deck;
};

using ReplayDeckList = std::vector<ReplayDeck>;
using ReplayStream = std::vector<CoreUtils::Packet>;

struct ReplayResponse {
public:
	uint8_t length;
	std::vector<uint8_t> response;
};

class Replay {
public:
	void BeginRecord(bool write = true, epro::path_string name = EPRO_TEXT("./replay/_LastReplay.yrpX"));
	void WriteStream(const ReplayStream& stream);
	void WritePacket(const CoreUtils::Packet& p);
	bool IsStreamedReplay();
	bool CanBePlayedInOldMode();
	bool CanBePlayedInStreamedMode() {
		return IsStreamedReplay();
	}
	bool HasPlayableYrp() {
		return yrp != nullptr && yrp->CanBePlayedInOldMode();
	}
	bool IsOldReplayMode() {
		return pheader.base.id == REPLAY_YRP1;
	}
	template<typename T>
	void Write(const T& data, bool flush = true);
	void WritetoFile(const void* data, size_t size, bool flush);
	void WriteHeader(ExtendedReplayHeader& header);
	void WriteData(const void* data, size_t length, bool flush = true);
	void Flush();
	void EndRecord(size_t size = 0x20000);
	void SaveReplay(const epro::path_string& name);
	bool OpenReplay(const epro::path_string& name);
	bool OpenReplayFromBuffer(std::vector<uint8_t>&& contents);
	bool IsExportable();
	static bool DeleteReplay(const epro::path_string& name);
	static bool RenameReplay(const epro::path_string& oldname, const epro::path_string& newname);
	bool GetNextResponse(ReplayResponse*& res);
	const std::vector<std::wstring>& GetPlayerNames();
	const ReplayDeckList& GetPlayerDecks();
	const std::vector<uint32_t>& GetRuleCards();
	void Rewind();
	void Reset();
	int GetPlayersCount(int side);
	int GetTurnsCount();
	epro::path_string GetReplayName();
	std::unique_ptr<Replay> yrp;
	std::vector<uint8_t> replay_data;
	std::vector<uint8_t> comp_data;
	std::vector<uint8_t> GetSerializedBuffer();
	ExtendedReplayHeader pheader;
	struct duel_parameters {
		uint32_t start_lp;
		uint32_t start_hand;
		uint32_t draw_count;
		uint64_t duel_flags;
	};
	duel_parameters params;
	std::string scriptname;
	ReplayStream packets_stream;
private:
	bool ReadData(void* data, uint32_t length);
	bool ReadData(std::vector<uint8_t>& data, uint32_t length);
	template <typename  T>
	T Read();
	bool ReadNextResponse(ReplayResponse* res);
	bool ReadName(wchar_t* data);
	bool ReadNextPacket(CoreUtils::Packet* packet);
	FILE* fp{ nullptr };
	size_t data_position;
	void ParseNames();
	void ParseParams();
	void ParseDecks();
	void ParseStream();
	bool ParseResponses();
	bool is_recording;
	bool is_replaying;
	bool can_read;
	std::vector<ReplayResponse> responses;
	std::vector<std::wstring> players;
	uint32_t home_count;
	uint32_t opposing_count;
	epro::path_string replay_name;
	ReplayDeckList decks;
	std::vector<uint32_t> replay_custom_rule_cards;
	std::vector<ReplayResponse>::iterator responses_iterator;
	int turn_count;
};
template<typename T>
inline void Replay::Write(const T& data, bool flush) {
	WriteData(&data, sizeof(T), flush);
}

}

#endif
