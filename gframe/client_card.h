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

}

#endif //CLIENT_CARD_H
