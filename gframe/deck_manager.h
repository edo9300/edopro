#ifndef DECKMANAGER_H
#define DECKMANAGER_H

#include "config.h"
#include "utils.h"
#include "client_card.h"
#include <unordered_map>
#include <vector>
#include "network.h"

namespace ygo {

struct LFList {
	unsigned int hash;
	std::wstring listName;
	std::unordered_map<uint32_t, int> content;
	bool whitelist;
};
struct Deck {
	std::vector<CardDataC*> main;
	std::vector<CardDataC*> extra;
	std::vector<CardDataC*> side;
	Deck() {}
	Deck(const Deck& ndeck) {
		main = ndeck.main;
		extra = ndeck.extra;
		side = ndeck.side;
	}
	void clear() {
		main.clear();
		extra.clear();
		side.clear();
	}
};
enum class DuelAllowedCards {
	ALLOWED_CARDS_OCG_ONLY,
	ALLOWED_CARDS_TCG_ONLY,
	ALLOWED_CARDS_OCG_TCG,
	ALLOWED_CARDS_WITH_PRERELEASE,
	ALLOWED_CARDS_ANY
};
class DeckManager {
public:
	std::vector<LFList> _lfList;

	void LoadLFListSingle(const path_string& path);
	bool LoadLFListFolder(path_string path);
	void LoadLFList();
	DeckError CheckDeck(Deck& deck, int lfhash, DuelAllowedCards allowedCards, bool doubled, int forbiddentypes = 0, bool is_speed = false);
	int TypeCount(std::vector<CardDataC*> cards, int type);
	int LoadDeck(Deck& deck, int* dbuf, int mainc, int sidec, int mainc2 = 0, int sidec2 = 0);
	int LoadDeck(Deck& deck, std::vector<int> mainlist, std::vector<int> sidelist);
	bool LoadSide(Deck& deck, int* dbuf, int mainc, int sidec);
};

extern DeckManager deckManager;

}

#endif //DECKMANAGER_H
