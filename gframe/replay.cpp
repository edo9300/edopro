#include "replay.h"
#include <algorithm>
#include <fstream>
#include "lzma/LzmaLib.h"

namespace ygo {

ReplayPacket::ReplayPacket(const CoreUtils::Packet& packet) {
	char* buf = (char*)packet.data.data();
	int msg = BufferIO::Read<uint8_t>(buf);
	Set(msg, buf, (int)(packet.data.size() - sizeof(uint8_t)));
}
ReplayPacket::ReplayPacket(char* buf, int len) {
	int msg = BufferIO::Read<uint8_t>(buf);
	Set(msg, buf, len);
}
ReplayPacket::ReplayPacket(int msg, char* buf, int len) {
	Set(msg, buf, len);
}
void ReplayPacket::Set(int msg, char* buf, int len) {
	message = msg;
	data.resize(len);
	if(len)
		memcpy(data.data(), buf, data.size());
}

Replay::Replay() {
	yrp = nullptr;
	is_recording = false;
	is_replaying = false;
	can_read = false;
}
Replay::~Replay() {
}
void Replay::BeginRecord(path_string name) {
	replay_data.clear();
	if(fp.is_open())
		fp.close();
	is_recording = false;
	if(name.size()>5) {
		fp.open(TEXT("./replay/") + name, std::ofstream::binary);
		if(!fp.is_open()) {
			return;
		}
	}
	is_recording = true;
}
void Replay::WritePacket(const ReplayPacket& p) {
	Write<int8_t>(p.message, false);
	Write<int32_t>(p.data.size(), false);
	WriteData((char*)p.data.data(), p.data.size());
}
void Replay::WriteStream(const ReplayStream& stream) {
	for(auto& packet : stream)
		WritePacket(packet);
}
void Replay::WritetoFile(const void* data, size_t size, bool flush) {
	if(!fp.is_open()) return;
	fp.write((char*)data, size);
	if(flush)
		fp.flush();
}
void Replay::WriteHeader(ReplayHeader& header) {
	pheader = header;
	Write<ReplayHeader>(header, true);
}
void Replay::WriteData(const void* data, unsigned int length, bool flush) {
	if(!is_recording)
		return;
	const auto vec_size = replay_data.size();
	replay_data.resize(vec_size + length);
	if(length)
		std::memcpy(&replay_data[vec_size], data, length);
	WritetoFile(data, length, flush);
}
void Replay::Flush() {
	if(!is_recording)
		return;
	if(!fp.is_open()) return;
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
	comp_size = size;
	comp_data.resize(replay_data.size() * 2);
	LzmaCompress(comp_data.data(), &comp_size, replay_data.data() + sizeof(ReplayHeader), replay_data.size() - sizeof(ReplayHeader), pheader.props, &propsize, 5, 1 << 24, 3, 0, 2, 32, 1);
	comp_data.resize(comp_size);
	is_recording = false;
}
}
