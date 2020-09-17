#include "replay.h"
#include <algorithm>
#include <fstream>
#include <fmt/format.h>
#include "lzma/LzmaLib.h"
#include "common.h"
#include "utils.h"

namespace ygo {
ReplayPacket::ReplayPacket(const CoreUtils::Packet& packet) {
	char* buf = (char*)packet.data.data();
	uint8_t msg = BufferIO::Read<uint8_t>(buf);
	Set(msg, buf, (uint32_t)(packet.data.size() - sizeof(uint8_t)));
}
ReplayPacket::ReplayPacket(char* buf, uint32_t len) {
	uint8_t msg = BufferIO::Read<uint8_t>(buf);
	Set(msg, buf, len);
}
ReplayPacket::ReplayPacket(uint8_t msg, char* buf, uint32_t len) {
	Set(msg, buf, len);
}
void ReplayPacket::Set(uint8_t msg, char* buf, uint32_t len) {
	message = msg;
	data.resize(len);
	if(len)
		memcpy(data.data(), buf, data.size());
}
void Replay::BeginRecord(bool write, path_string name) {
	Reset();
	if(fp.is_open())
		fp.close();
	is_recording = false;
	if(write) {
		fp.open(name, std::ofstream::binary);
		if(!fp.is_open()) {
			return;
		}
	}
	is_recording = true;
}
void Replay::WritePacket(const ReplayPacket& p) {
	Write<uint8_t>(p.message, false);
	Write<uint32_t>(p.data.size(), false);
	WriteData((char*)p.data.data(), p.data.size());
}
void Replay::WriteStream(const ReplayStream& stream) {
	for(auto& packet : stream)
		WritePacket(packet);
}
void Replay::WritetoFile(const void* data, size_t size, bool flush){
	if(!fp.is_open()) return;
	fp.write((char*)data, size);
	if(flush)
		fp.flush();
}
void Replay::WriteHeader(ReplayHeader& header) {
	pheader = header;
	Write<ReplayHeader>(header, true);
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
	if(!is_recording)
		return;
	if(!fp.is_open())
		return;
	fp.flush();
}
void Replay::EndRecord(size_t size) {
	if(!is_recording)
		return;
	if(fp.is_open())
		fp.close();
	pheader.datasize = replay_data.size() - sizeof(ReplayHeader);
	pheader.flag |= REPLAY_COMPRESSED;
	size_t propsize = 5;
	auto comp_size = size;
	comp_data.resize(replay_data.size() * 2);
	LzmaCompress(comp_data.data(), &comp_size, replay_data.data() + sizeof(ReplayHeader), pheader.datasize, pheader.props, &propsize, 5, 1 << 24, 3, 0, 2, 32, 1);
	comp_data.resize(comp_size);
	is_recording = false;
}
void Replay::SaveReplay(const path_string& name) {
	std::ofstream replay_file(fmt::format(EPRO_TEXT("./replay/{}.yrpX"), name.c_str()), std::ofstream::binary);
	if(!replay_file.is_open())
		return;
	replay_file.write((char*)&pheader, sizeof(pheader));
	replay_file.write((char*)comp_data.data(), comp_data.size());
	replay_file.close();
}
bool Replay::OpenReplayFromBuffer(std::vector<uint8_t>&& contents) {
	Reset();
	memcpy(&pheader, contents.data(), sizeof(pheader));
	if(pheader.id != REPLAY_YRP1 && pheader.id != REPLAY_YRPX) {
		Reset();
		return false;
	}
	if(pheader.flag & REPLAY_COMPRESSED) {
		size_t replay_size = pheader.datasize;
		auto comp_size = contents.size() - sizeof(ReplayHeader);
		replay_data.resize(pheader.datasize);
		if(LzmaUncompress(replay_data.data(), &replay_size, contents.data() + sizeof(ReplayHeader), &comp_size, pheader.props, 5) != SZ_OK)
			return false;
	} else {
		contents.erase(contents.begin(), contents.begin() + sizeof(pheader));
		replay_data = std::move(contents);
	}
	data_position = 0;
	is_replaying = true;
	can_read = true;
	ParseNames();
	ParseParams();
	if(pheader.id == REPLAY_YRP1) {
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
bool Replay::OpenReplay(const path_string& name) {
	if(replay_name == name) {
		Rewind();
		return true;
	}
	Reset();
	std::ifstream replay_file(name, std::ifstream::binary);
	if(!replay_file.is_open()) {
		replay_file.open(EPRO_TEXT("./replay/") + name, std::ifstream::binary);
		if(!replay_file.is_open()) {
			replay_name.clear();
			return false;
		}
	}
	std::vector<uint8_t> contents((std::istreambuf_iterator<char>(replay_file)), std::istreambuf_iterator<char>());
	replay_file.close();
	if (OpenReplayFromBuffer(std::move(contents))){
		replay_name = name;
		return true;
	}
	replay_name.clear();
	return false;
}
bool Replay::DeleteReplay(const path_string& name) {
	return Utils::FileDelete(name);
}
bool Replay::RenameReplay(const path_string& oldname, const path_string& newname) {
	return Utils::FileMove(oldname, newname);
}
bool Replay::GetNextResponse(ReplayResponse* res) {
	if(responses_iterator == responses.end())
		return false;
	*res = *responses_iterator;
	responses_iterator++;
	return true;
}
const std::vector<std::wstring>& Replay::GetPlayerNames() {
	return players;
}
const ReplayDeckList& Replay::GetPlayerDecks() {
	if(pheader.id == REPLAY_YRPX && yrp)
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
	if(pheader.flag & REPLAY_SINGLE_MODE) {
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
		if(pheader.flag & REPLAY_NEWREPLAY)
			count = Read<uint32_t>();
		else if(pheader.flag & REPLAY_TAG)
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
	if(pheader.id == REPLAY_YRP1) {
		params.start_lp = Read<uint32_t>();
		params.start_hand = Read<uint32_t>();
		params.draw_count = Read<uint32_t>();
	}
	params.duel_flags = Read<uint32_t>();
	if(pheader.flag & REPLAY_SINGLE_MODE && pheader.id == REPLAY_YRP1) {
		size_t slen = Read<uint16_t>();
		scriptname.resize(slen);
		ReadData(&scriptname[0], slen);
	}
}
void Replay::ParseDecks() {
	decks.clear();
	if(pheader.id != REPLAY_YRP1 || (pheader.flag & REPLAY_SINGLE_MODE && !(pheader.flag & REPLAY_HAND_TEST)))
		return;
	for(uint32_t i = 0; i < home_count + opposing_count; i++) {
		ReplayDeck tmp;
		for(uint32_t i = 0, main = Read<uint32_t>(); i < main && can_read; ++i)
			tmp.main_deck.push_back(Read<uint32_t>());
		for(uint32_t i = 0, extra = Read<uint32_t>(); i < extra && can_read; ++i)
			tmp.extra_deck.push_back(Read<uint32_t>());
		decks.push_back(tmp);
	}
	replay_custom_rule_cards.clear();
	if(pheader.flag & REPLAY_NEWREPLAY && !(pheader.flag & REPLAY_HAND_TEST)) {
		uint32_t rules = Read<uint32_t>();
		for(uint32_t i = 0; i < rules && can_read; ++i)
			replay_custom_rule_cards.push_back(Read<uint32_t>());
	}
}
bool Replay::ReadNextPacket(ReplayPacket* packet) {
	if(!can_read)
		return false;
	uint8_t message = Read<uint8_t>();
	if(!can_read)
		return false;
	packet->message = message;
	uint32_t len = Read<uint32_t>();
	if(!can_read)
		return false;
	return ReadData(packet->data, len);
}
void Replay::ParseStream() {
	packets_stream.clear();
	if(pheader.id != REPLAY_YRPX)
		return;
	ReplayPacket p;
	while(ReadNextPacket(&p)) {
		if(p.message == MSG_AI_NAME) {
			char* pbuf = (char*)p.data.data();
			int len = BufferIO::Read<uint16_t>(pbuf);
			if(!can_read)
				break;
			std::string namebuf;
			namebuf.resize(len);
			memcpy(&namebuf[0], pbuf, len + 1);
			players[1] = BufferIO::DecodeUTF8s(namebuf);
			continue;
		}
		if(p.message == MSG_NEW_TURN) {
			turn_count++;
		}
		if(p.message == OLD_REPLAY_MODE) {
			if(!yrp) {
				yrp = std::unique_ptr<Replay>(new Replay{});
				if(!yrp->OpenReplayFromBuffer(std::move(p.data)))
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
	BufferIO::CopyWStr(buffer, data, 20);
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
path_string Replay::GetReplayName() {
	return replay_name;
}
std::vector<uint8_t> Replay::GetSerializedBuffer() {
	std::vector<uint8_t> serialized;
	serialized.resize(sizeof(ReplayHeader));
	memcpy(serialized.data(), &pheader, sizeof(ReplayHeader));
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
	if(pheader.id != REPLAY_YRP1)
		return false;
	ReplayResponse r;
	while(ReadNextResponse(&r)) {
		responses.push_back(r);
	}
	responses_iterator = responses.begin();
	return !responses.empty();
}

}
