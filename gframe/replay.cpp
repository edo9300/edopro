#include "replay.h"
#include <algorithm>
#include "lzma/LzmaLib.h"
#include "common.h"
#include "utils.h"
#include "file_stream.h"
#include "fmt.h"

namespace ygo {
void Replay::BeginRecord(bool write, epro::path_string name) {
	Reset();
	if(fp != nullptr) {
		fclose(fp);
		fp = nullptr;
	}
	is_recording = true;
	if(write) {
		fp = fileopen(name.data(), "wb");
		is_recording = fp != nullptr;
	}
}
void Replay::WritePacket(const CoreUtils::Packet& p) {
	Write<uint8_t>(p.message, false);
	Write<uint32_t>(static_cast<uint32_t>(p.buff_size()), false);
	WriteData(p.data(), p.buff_size());
}
bool Replay::IsStreamedReplay() {
	return pheader.base.id == REPLAY_YRPX;
}
bool Replay::CanBePlayedInOldMode() {
	return pheader.header_version == 1;
}
void Replay::WriteStream(const ReplayStream& stream) {
	for(auto& packet : stream)
		WritePacket(packet);
}
void Replay::WritetoFile(const void* data, size_t size, bool flush){
	if(fp == nullptr)
		return;
	fwrite(data, 1, size, fp);
	if(flush)
		fflush(fp);
}
void Replay::WriteHeader(ExtendedReplayHeader& header) {
	pheader = header;
	Write<ExtendedReplayHeader>(header, true);
}
void Replay::WriteData(const void* data, size_t length, bool flush) {
	if(!is_recording)
		return;
	if(!length)
		return;
	const auto vec_size = replay_data.size();
	replay_data.resize(vec_size + length);
	std::memcpy(&replay_data[vec_size], data, length);
	WritetoFile(data, length, flush);
}
void Replay::Flush() {
	if(!is_recording || fp == nullptr)
		return;
	fflush(fp);
}
void Replay::EndRecord(size_t size) {
	if(!is_recording)
		return;
	if(fp != nullptr) {
		fclose(fp);
		fp = nullptr;
	}
	pheader.base.datasize = static_cast<uint32_t>(replay_data.size() - sizeof(ExtendedReplayHeader));
	pheader.base.flag |= REPLAY_COMPRESSED;
	size_t propsize = 5;
	auto comp_size = size;
	comp_data.resize(replay_data.size() * 2);
	LzmaCompress(comp_data.data(), &comp_size, replay_data.data() + sizeof(ExtendedReplayHeader), pheader.base.datasize, pheader.base.props, &propsize, 5, 1 << 24, 3, 0, 2, 32, 1);
	comp_data.resize(comp_size);
	is_recording = false;
}
void Replay::SaveReplay(const epro::path_string& name) {
	auto replay_file = fileopen(epro::format(EPRO_TEXT("./replay/{}.yrpX"), name).data(), "wb");
	if(replay_file == nullptr)
		return;
	auto header_len = (pheader.base.flag & REPLAY_EXTENDED_HEADER) ? sizeof(ExtendedReplayHeader) : sizeof(ReplayHeader);
	fwrite(&pheader, 1, header_len, replay_file);
	fwrite(comp_data.data(), 1, comp_data.size(), replay_file);
	fclose(replay_file);
}
static inline bool IsReplayValid(uint32_t id) {
	return id == REPLAY_YRP1 || id == REPLAY_YRPX;
}
bool Replay::OpenReplayFromBuffer(std::vector<uint8_t>&& contents) {
	Reset();
	uint32_t header_size;
	if(!ExtendedReplayHeader::ParseReplayHeader(contents.data(), contents.size(), pheader, &header_size) || !IsReplayValid(pheader.base.id)) {
		Reset();
		return false;
	}
	if(pheader.base.flag & REPLAY_COMPRESSED) {
		size_t replay_size = pheader.base.datasize;
		auto comp_size = contents.size() - header_size;
		replay_data.resize(pheader.base.datasize);
		if(LzmaUncompress(replay_data.data(), &replay_size, contents.data() + header_size, &comp_size, pheader.base.props, 5) != SZ_OK)
			return false;
	} else {
		contents.erase(contents.begin(), contents.begin() + header_size);
		replay_data = std::move(contents);
	}
	data_position = 0;
	is_replaying = true;
	can_read = true;
	ParseNames();
	ParseParams();
	if(pheader.base.id == REPLAY_YRP1) {
		ParseDecks();
		ParseResponses();
	} else {
		ParseStream();
	}
	return true;
}
bool Replay::IsExportable() {
	auto& deck = (yrp != nullptr) ? yrp->GetPlayerDecks() : decks;
	if(players.empty() || deck.empty() || players.size() > deck.size())
		return false;
	return true;
}
bool Replay::OpenReplay(const epro::path_string& name) {
	if(replay_name == name) {
		Rewind();
		return true;
	}
	Reset();
	std::vector<uint8_t> contents;
	FileStream replay_file{ name, FileStream::in | FileStream::binary };
	if(replay_file.fail()) {
		FileStream replay_file2{ EPRO_TEXT("./replay/") + name, FileStream::in | FileStream::binary };
		if(replay_file2.fail()) {
			replay_name.clear();
			return false;
		}
		contents.assign(std::istreambuf_iterator<char>(replay_file2), std::istreambuf_iterator<char>());
	} else
		contents.assign(std::istreambuf_iterator<char>(replay_file), std::istreambuf_iterator<char>());
	if (OpenReplayFromBuffer(std::move(contents))){
		replay_name = name;
		return true;
	}
	replay_name.clear();
	return false;
}
bool Replay::DeleteReplay(const epro::path_string& name) {
	return Utils::FileDelete(name);
}
bool Replay::RenameReplay(const epro::path_string& oldname, const epro::path_string& newname) {
	return Utils::FileMove(oldname, newname);
}
bool Replay::GetNextResponse(ReplayResponse*& res) {
	if(responses_iterator == responses.end())
		return false;
	res = &*responses_iterator;
	responses_iterator++;
	return true;
}
const std::vector<std::wstring>& Replay::GetPlayerNames() {
	return players;
}
const ReplayDeckList& Replay::GetPlayerDecks() {
	if(IsStreamedReplay() && yrp)
		return yrp->decks;
	return decks;
}
const std::vector<uint32_t>& Replay::GetRuleCards() {
	return replay_custom_rule_cards;
}
bool Replay::ReadNextResponse(ReplayResponse* res) {
	if(!can_read || !res)
		return false;
	res->length = Read<uint8_t>();
	if(!res->length)
		return false;
	return ReadData(res->response, res->length);
}
void Replay::ParseNames() {
	players.clear();
	if(pheader.base.flag & REPLAY_SINGLE_MODE) {
		wchar_t namebuf[20];
		ReadName(namebuf);
		players.push_back(namebuf);
		ReadName(namebuf);
		players.push_back(namebuf);
		home_count = 1;
		opposing_count = 1;
		return;
	}
	auto f = [this](uint32_t& count) {
		if(pheader.base.flag & REPLAY_NEWREPLAY)
			count = Read<uint32_t>();
		else if(pheader.base.flag & REPLAY_TAG)
			count = 2;
		else
			count = 1;
		for(uint32_t i = 0; i < count; i++) {
			wchar_t namebuf[20];
			ReadName(namebuf);
			players.push_back(namebuf);
		}
	};
	f(home_count);
	f(opposing_count);
}
void Replay::ParseParams() {
	params = { 0 };
	if(pheader.base.id == REPLAY_YRP1) {
		params.start_lp = Read<uint32_t>();
		params.start_hand = Read<uint32_t>();
		params.draw_count = Read<uint32_t>();
	}
	if(pheader.base.flag & REPLAY_64BIT_DUELFLAG)
		params.duel_flags = Read<uint64_t>();
	else
		params.duel_flags = Read<uint32_t>();
	if(pheader.base.flag & REPLAY_SINGLE_MODE && pheader.base.id == REPLAY_YRP1) {
		uint16_t slen = Read<uint16_t>();
		scriptname.resize(slen);
		ReadData(&scriptname[0], slen);
	}
}
void Replay::ParseDecks() {
	decks.clear();
	if(pheader.base.id != REPLAY_YRP1 || (pheader.base.flag & REPLAY_SINGLE_MODE && !(pheader.base.flag & REPLAY_HAND_TEST)))
		return;
	for(uint32_t i = 0; i < home_count + opposing_count; ++i) {
		ReplayDeck tmp;
		for(uint32_t j = 0, main = Read<uint32_t>(); j < main && can_read; ++j)
			tmp.main_deck.push_back(Read<uint32_t>());
		for(uint32_t j = 0, extra = Read<uint32_t>(); j < extra && can_read; ++j)
			tmp.extra_deck.push_back(Read<uint32_t>());
		decks.push_back(std::move(tmp));
	}
	replay_custom_rule_cards.clear();
	if(pheader.base.flag & REPLAY_NEWREPLAY && !(pheader.base.flag & REPLAY_HAND_TEST)) {
		uint32_t rules = Read<uint32_t>();
		for(uint32_t i = 0; i < rules && can_read; ++i)
			replay_custom_rule_cards.push_back(Read<uint32_t>());
	}
}
bool Replay::ReadNextPacket(CoreUtils::Packet* packet) {
	if(!can_read)
		return false;
	uint8_t message = Read<uint8_t>();
	if(!can_read)
		return false;
	packet->message = message;
	uint32_t len = Read<uint32_t>();
	if(!can_read)
		return false;
	return ReadData(packet->buffer, len);
}
void Replay::ParseStream() {
	packets_stream.clear();
	if(!IsStreamedReplay())
		return;
	CoreUtils::Packet p;
	while(ReadNextPacket(&p)) {
		if(p.message == MSG_AI_NAME) {
			auto* pbuf = p.data();
			uint16_t len = BufferIO::Read<uint16_t>(pbuf);
			if((len + 1u) != p.buff_size() - sizeof(uint16_t))
				break;
			pbuf[len] = 0;
			players[1] = BufferIO::DecodeUTF8({ reinterpret_cast<char*>(pbuf), len });
			continue;
		}
		if(p.message == MSG_NEW_TURN) {
			turn_count++;
		}
		if(p.message == OLD_REPLAY_MODE) {
			if(!yrp) {
				yrp = std::make_unique<Replay>();
				if(!yrp->OpenReplayFromBuffer(std::move(p.buffer)))
					yrp = nullptr;
			}
			continue;
		}
		packets_stream.push_back(p);
	}
}
bool Replay::ReadName(wchar_t* data) {
	if(!is_replaying || !can_read)
		return false;
	uint16_t buffer[20];
	if(!ReadData(buffer, 40))
		return false;
	BufferIO::DecodeUTF16(buffer, data, 20);
	return true;
}
void Replay::Reset() {
	yrp = nullptr;
	scriptname.clear();
	responses.clear();
	responses.shrink_to_fit();
	players.clear();
	decks.clear();
	decks.shrink_to_fit();
	params = { 0 };
	packets_stream.clear();
	packets_stream.shrink_to_fit();
	data_position = 0;
	replay_data.clear();
	replay_data.shrink_to_fit();
	comp_data.clear();
	comp_data.shrink_to_fit();
	turn_count = 0;
}
int Replay::GetPlayersCount(int side) {
	if(side == 0)
		return home_count;
	return opposing_count;
}
int Replay::GetTurnsCount() {
	return turn_count;
}
epro::path_string Replay::GetReplayName() {
	return replay_name;
}
std::vector<uint8_t> Replay::GetSerializedBuffer() {
	std::vector<uint8_t> serialized;
	serialized.resize(sizeof(ExtendedReplayHeader));
	memcpy(serialized.data(), &pheader, sizeof(ExtendedReplayHeader));
	serialized.insert(serialized.end(), comp_data.begin(), comp_data.end());
	return serialized;
}
bool Replay::ReadData(void* data, uint32_t length) {
	if(!is_replaying || !can_read)
		return false;
	if((replay_data.size() - data_position) < length) {
		can_read = false;
		return false;
	}
	if(length)
		memcpy(data, &replay_data[data_position], length);
	data_position += length;
	return true;
}
bool Replay::ReadData(std::vector<uint8_t>& data, uint32_t length) {
	if(!is_replaying || !can_read)
		return false;
	if((replay_data.size() - data_position) < length) {
		can_read = false;
		return false;
	}
	if(length) {
		data.resize(length);
		memcpy(data.data(), &replay_data[data_position], length);
		data_position += length;
	}
	return true;
}
template<typename T>
T Replay::Read() {
	T ret = 0;
	ReadData(&ret, sizeof(T));
	return ret;
}
void Replay::Rewind() {
	data_position = 0;
	responses_iterator = responses.begin();
	if(yrp)
		yrp->Rewind();
}
bool Replay::ParseResponses() {
	responses.clear();
	if(pheader.base.id != REPLAY_YRP1)
		return false;
	ReplayResponse r;
	while(ReadNextResponse(&r)) {
		responses.push_back(r);
	}
	responses_iterator = responses.begin();
	return !responses.empty();
}

bool ExtendedReplayHeader::ParseReplayHeader(const void* data, size_t input_len, ExtendedReplayHeader& header, uint32_t* header_length) {
	if(input_len < sizeof(ReplayHeader))
		return false;
	ExtendedReplayHeader ret{};
	memcpy(&ret.base, data, sizeof(ReplayHeader));
	if(header_length)
		*header_length = sizeof(ReplayHeader);
	if(ret.base.flag & REPLAY_EXTENDED_HEADER) {
		// for now there's only this "revision" of the header, so this will be the minimal extra size
		// in future this check will have to be improved
		if(input_len < sizeof(ExtendedReplayHeader))
			return false;
		*header_length = sizeof(ExtendedReplayHeader);
		memcpy(&ret, data, sizeof(ExtendedReplayHeader));
		if(ret.header_version > ExtendedReplayHeader::latest_header_version)
			return false;
	}
	header = ret;
	return true;
}

}
