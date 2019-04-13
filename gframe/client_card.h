#ifndef CLIENT_CARD_H
#define CLIENT_CARD_H

#include "bufferio.h"
#include <vector>
#include <set>
#include <map>
#include <unordered_map>

namespace ygo {

struct CardData {
	unsigned int code;
	unsigned int alias;
	unsigned long long setcode;
	unsigned int type;
	unsigned int level;
	unsigned int attribute;
	unsigned int race;
	int attack;
	int defense;
	unsigned int lscale;
	unsigned int rscale;
	unsigned int link_marker;
};
struct CardDataC {
	unsigned int code;
	unsigned int alias;
	unsigned long long setcode;
	unsigned int type;
	int level;
	unsigned int attribute;
	unsigned int race;
	int attack;
	int defense;
	unsigned int lscale;
	unsigned int rscale;
	unsigned int link_marker;
	unsigned int ot;
	unsigned int category;
};
struct loc_info {
	uint8_t controler;
	uint8_t location;
	uint32_t sequence;
	uint32_t position;
};
inline static loc_info read_location_info(char*& p) {
	loc_info info;
	info.controler = BufferIO::ReadInt8(p);
	info.location = BufferIO::ReadInt8(p);
	info.sequence = BufferIO::ReadInt32(p);
	info.position = BufferIO::ReadInt32(p);
	return info;
}

}

#endif //CLIENT_CARD_H
