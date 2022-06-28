#include "replay.h"
#include <algorithm>
#include <fmt/format.h>
#include <lzma.h>
#include "common.h"
#include "utils.h"
#include "file_stream.h"

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
	size_t comp_size = 0;
	comp_data.resize(replay_data.size() * 2);

	lzma_options_lzma opts;
	lzma_lzma_preset(&opts, 5);
	opts.dict_size = 1 << 24;
	lzma_filter filters[]{
		{ LZMA_FILTER_LZMA1, &opts },
		{ LZMA_VLI_UNKNOWN,  nullptr},
	};

	lzma_properties_encode(filters, pheader.base.props);
	lzma_ret ret;
	ret = lzma_raw_buffer_encode(filters, nullptr, replay_data.data() + sizeof(ExtendedReplayHeader), pheader.base.datasize, comp_data.data(), &comp_size, comp_data.size());
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
		size_t comp_size = contents.size() - header_size;
		replay_data.resize(replay_size);

		const auto fake_header = [&]() {
			/* the lzma header consists of :
				1 byte   LZMA properties byte that encodes lc/lp/pb
				4 bytes  dictionary size as little endian uint32_t
				8 bytes  uncompressed size as little endian uint64_t

				with the first 5 bytes corresponding to the "props"
				stored in the replay header
			*/
			std::array<uint8_t, sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint64_t)> header;
			memcpy(header.data(), pheader.base.props, 5);
			header[5] = (pheader.base.datasize >> 8 * 0) & 0xff;
			header[6] = (pheader.base.datasize >> 8 * 1) & 0xff;
			header[7] = (pheader.base.datasize >> 8 * 2) & 0xff;
			header[8] = (pheader.base.datasize >> 8 * 3) & 0xff;
			header[9] = 0;
			header[10] = 0;
			header[11] = 0;
			header[12] = 0;
			return header;
		}();

		lzma_stream stream = LZMA_STREAM_INIT;
		stream.avail_in = fake_header.size();
		stream.next_in = fake_header.data();

		stream.avail_out = pheader.base.datasize;
		stream.next_out = replay_data.data();

		if(lzma_alone_decoder(&stream, UINT64_MAX) != LZMA_OK) {
			Reset();
			return false;
		}

		while(stream.avail_in != 0) {
			// this is should only feed the fake header, if for some reasons
			// LZMA_STREAM_END is returned, then something went wrong
			if(lzma_code(&stream, LZMA_RUN) != LZMA_OK) {
				lzma_end(&stream);
				Reset();
				return false;
			}
		}

		if(stream.total_out != 0) {
			lzma_end(&stream);
			Reset();
			return false;
		}

		stream.avail_in = comp_size;
		stream.next_in = contents.data() + header_size;

		while(stream.avail_in != 0) {
			auto ret = lzma_code(&stream, LZMA_RUN);
			if(ret == LZMA_STREAM_END) {
				if(stream.total_out != pheader.base.datasize) {
					lzma_end(&stream);
					Reset();
					return false;
				}
				break;
			}
			if(ret != LZMA_OK) {
				// if liblzma finds both the header and the end of stream marker, it returns
				// LZMA_DATA_ERROR, we ignore that error and just ensure that the total written
				// size matches the uncompressed size
				if(ret == LZMA_DATA_ERROR && stream.total_out == pheader.base.datasize)
					break;
				Reset();
				lzma_end(&stream);
				return false;
			}
		}
		lzma_end(&stream);
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
				yrp = std::unique_ptr<Replay>(new Replay{});
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
