#ifndef DECKMANAGER_H
#define DECKMANAGER_H

#include <unordered_map>
#include <vector>
#include <map>
#include "network.h"
#include "text_types.h"
#include "data_manager.h"
#include "deck.h"

namespace ygo {

using banlist_content_t = std::unordered_map<uint32_t, int>;
using cardlist_type = std::vector<uint32_t>;

struct LFList {
	uint32_t hash;
	std::wstring listName;
	banlist_content_t content;
	bool whitelist;
	auto GetLimitationIterator(const CardDataC* pcard) const {
		auto flit = content.find(pcard->code);
		if(flit == content.end() && pcard->alias) {
			if(!whitelist || pcard->IsInArtworkOffsetRange())
				flit = content.find(pcard->alias);
		}
		return flit;
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
	size_t null_lflist_index = ~size_t();
	mutable std::unordered_map<uint32_t, CardDataC*> dummy_entries;
	const CardDataC* GetDummyOrMappedCardData(uint32_t code) const;
	bool load_dummies{ true };
public:
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
	bool LoadLFListFolder(epro::path_stringview path);
	void LoadLFList();
	void RefreshLFList();
	LFList const* GetLFList(uint32_t lfhash) const;
	epro::wstringview GetLFListName(uint32_t lfhash) const;
	static void RefreshDeck(Deck& deck);
	static DeckError CheckDeckContent(const Deck& deck, LFList const* lflist, DuelAllowedCards allowedCards, uint32_t forbiddentypes, bool rituals_in_extra);
	static DeckError CheckDeckSize(const Deck& deck, const DeckSizes& sizes);
	static int TypeCount(const Deck::Vector& cards, uint32_t type);
	static int CountLegends(const Deck::Vector& cards, uint32_t type);
	static uint32_t LoadDeckFromBuffer(Deck& deck, uint32_t* dbuf, uint32_t mainc, uint32_t sidec, bool rituals_in_extra = false);
	static uint32_t LoadDeck(Deck& deck, const cardlist_type& mainlist, const cardlist_type& sidelist, const cardlist_type* extralist = nullptr, bool rituals_in_extra = false);
	static bool LoadSide(Deck& deck, uint32_t* dbuf, uint32_t mainc, uint32_t sidec, bool rituals_in_extra);
	static bool LoadDeckFromFile(epro::path_stringview file, Deck& out, bool separated = false);
	static bool SaveDeck(epro::path_stringview name, const Deck& deck);
	static bool SaveDeck(epro::path_stringview name, const cardlist_type& mainlist, const cardlist_type& extralist, const cardlist_type& sidelist);
	static std::string MakeYdkEntryString(uint32_t code);
	static const wchar_t* ExportDeckBase64(const Deck& deck);
	static const wchar_t* ExportDeckCardNames(Deck deck);
	static void ImportDeckBase64(Deck& deck, const wchar_t* buffer);
	static bool ImportDeckBase64Omega(Deck& deck, epro::wstringview buffer);
	static bool DeleteDeck(Deck& deck, epro::path_stringview name);
	static bool RenameDeck(epro::path_stringview oldname, epro::path_stringview newname);
};

extern DeckManager* gdeckManager;

}

#endif //DECKMANAGER_H
