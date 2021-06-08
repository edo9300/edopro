#ifndef DECKMANAGER_H
#define DECKMANAGER_H

#include <unordered_map>
#include <vector>
#include <map>
#include "network.h"
#include "text_types.h"
#include "data_manager.h"

namespace ygo {

using banlist_content_t = std::unordered_map<uint32_t, int>;
using cardlist_type = std::vector<uint32_t>;

struct LFList {
	uint32_t hash;
	std::wstring listName;
	banlist_content_t content;
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
private:
	int null_lflist_index = -1;
	std::unordered_map<uint32_t, CardDataC*> dummy_entries;
	CardDataC* GetDummyOrMappedCardData(uint32_t code);
	CardDataC* GetMappedCardData(uint32_t code);
	bool load_dummies{ true };
	std::map<uint32_t, uint32_t> mapped_ids;
public:
	Deck current_deck;
	Deck sent_deck;
	Deck pre_deck;
	std::vector<LFList> _lfList;
	~DeckManager() {
		ClearDummies();
	}
	void StopDummyLoading() {
		load_dummies = false;
	}
	void ClearDummies();
	bool LoadLFListSingle(const epro::path_string& path);
	bool LoadLFListFolder(epro::path_string path);
	void LoadLFList();
	void RefreshLFList();
	void RefreshDeck(Deck & deck);
	LFList* GetLFList(uint32_t lfhash);
	epro::wstringview GetLFListName(uint32_t lfhash);
	DeckError CheckDeck(Deck& deck, uint32_t lfhash, DuelAllowedCards allowedCards, bool doubled, uint32_t forbiddentypes = 0);
	int TypeCount(std::vector<CardDataC*> cards, uint32_t type);
	static uint32_t LoadDeck(Deck& deck, uint32_t* dbuf, uint32_t mainc, uint32_t sidec, uint32_t mainc2 = 0, uint32_t sidec2 = 0);
	static uint32_t LoadDeck(Deck& deck, const cardlist_type& mainlist, const cardlist_type& sidelist, const cardlist_type* extralist = nullptr);
	bool LoadSide(Deck& deck, uint32_t* dbuf, uint32_t mainc, uint32_t sidec);
	bool LoadDeck(epro::path_stringview file, Deck* deck = nullptr, bool separated = false);
	bool LoadDeckDouble(epro::path_stringview file, epro::path_stringview file2, Deck* deck = nullptr);
	bool SaveDeck(Deck& deck, epro::path_stringview name);
	bool SaveDeck(epro::path_stringview name, const cardlist_type& mainlist, const cardlist_type& extralist, const cardlist_type& sidelist);
	static const wchar_t* ExportDeckBase64(Deck& deck);
	static const wchar_t* ExportDeckCardNames(Deck deck);
	static void ImportDeckBase64(Deck& deck, const wchar_t* buffer);
	static bool ImportDeckBase64Omega(Deck& deck, epro::wstringview buffer);
	static bool DeleteDeck(Deck& deck, epro::path_stringview name);
	static bool RenameDeck(epro::path_stringview oldname, epro::path_stringview newname);
};

extern DeckManager* gdeckManager;

}

#endif //DECKMANAGER_H
