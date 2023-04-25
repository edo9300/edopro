#include <algorithm>
#include <fmt/format.h>
#include <zlib.h>
#include "network.h"
#include "deck_manager.h"
#include "data_manager.h"
#include "game.h"
#include <IGUIEditBox.h>
#include "Base64.h"
#include "utils.h"
#include "client_card.h"
#include "file_stream.h"

namespace ygo {
const CardDataC* DeckManager::GetDummyOrMappedCardData(uint32_t code) const {
	if(!load_dummies)
		return gDataManager->GetMappedCardData(code);
	auto it = dummy_entries.find(code);
	if(it != dummy_entries.end())
		return it->second;
	CardDataC* tmp = new CardDataC();
	tmp->code = 0;
	tmp->alias = code;
	dummy_entries[code] = tmp;
	return tmp;
}
void DeckManager::ClearDummies() {
	for(auto& card : dummy_entries) {
		delete card.second;
	}
	dummy_entries.clear();
}
bool DeckManager::LoadLFListSingle(const epro::path_string& path) {
	static constexpr auto key = "$whitelist"_sv;
	FileStream infile{ path, FileStream::in };
	if(infile.fail())
		return false;
	bool loaded = false;
	LFList lflist;
	lflist.hash = 0;
	std::string str;
	while(std::getline(infile, str)) {
		{
			auto pos = str.find('\r');
			if(pos != std::string::npos)
				str.erase(pos);
		}
		if(str.empty() || str[0] == '#')
			continue;
		if(str[0] == '!') {
			if(lflist.hash)
				_lfList.push_back(std::move(lflist));
			lflist.listName = BufferIO::DecodeUTF8({ str.data() + 1, str.size() - 1 });
			lflist.content.clear();
			lflist.hash = 0x7dfcee6a;
			lflist.whitelist = false;
			loaded = true;
			continue;
		}
		if(str.rfind(key.data(), 0, key.size()) == 0) {
			lflist.whitelist = true;
			continue;
		}
		if(!lflist.hash)
			continue;
		auto p = str.find(' ');
		if(p == std::string::npos)
			continue;
		auto c = str.find_first_not_of("-0123456789", p + 1);
		if(c != std::string::npos)
			c -= p;
		try {
			auto code = static_cast<uint32_t>(std::stoul(str.substr(0, p)));
			if(code == 0)
				continue;
			auto count = static_cast<int32_t>(std::stol(str.substr(p, c)));
			lflist.content[code] = count;
			lflist.hash = lflist.hash ^ ((code << 18) | (code >> 14)) ^ ((code << (27 + count)) | (code >> (5 - count)));
		}
		catch(...){}
	}
	if(lflist.hash)
		_lfList.push_back(lflist);
	return loaded;
}
bool DeckManager::LoadLFListFolder(epro::path_stringview _path) {
	auto path = Utils::NormalizePath(_path);
	bool loaded = false;
	auto lflists = Utils::FindFiles(path, { EPRO_TEXT("conf") });
	for (const auto& lflist : lflists) {
		loaded = LoadLFListSingle(path + lflist);
	}
	return loaded;
}
void DeckManager::LoadLFList() {
	LoadLFListSingle(EPRO_TEXT("./expansions/lflist.conf"));
	LoadLFListSingle(EPRO_TEXT("./lflist.conf"));
	LoadLFListFolder(EPRO_TEXT("./lflists/"));
	LFList nolimit;
	nolimit.listName = L"N/A"; // N/A
	nolimit.hash = 0;
	nolimit.content.clear();
	nolimit.whitelist = false;
	_lfList.push_back(nolimit);
	null_lflist_index = _lfList.size() - 1;
}
//moves the "N/A" lflist at the bottom of the vector
void DeckManager::RefreshLFList() {
	if (null_lflist_index != ~size_t() && null_lflist_index != _lfList.size() - 1) {
		auto it = _lfList.begin() + null_lflist_index;
		std::rotate(it, it + 1, _lfList.end());
		null_lflist_index = _lfList.size() - 1;
	}
}
void DeckManager::RefreshDeck(Deck& deck) {
	for(auto& list : { &deck.main, &deck.extra, &deck.side }) {
		for(auto& card : *list) {
			if(card->code == 0 && card->alias) {
				const CardDataC* cd;
				if((cd = gDataManager->GetCardData(card->alias)) == nullptr)
					cd = gDataManager->GetMappedCardData(card->alias);
				if(cd != nullptr)
					card = cd;
			}
		}
	}
}
LFList const* DeckManager::GetLFList(uint32_t lfhash) const {
	auto it = std::find_if(_lfList.begin(), _lfList.end(), [lfhash](LFList list) {return list.hash == lfhash; });
	return it != _lfList.end() ? &*it : nullptr;
}
epro::wstringview DeckManager::GetLFListName(uint32_t lfhash) const {
	auto lflist = GetLFList(lfhash);
	if(lflist)
		return lflist->listName;
	return gDataManager->unknown_string;
}
int DeckManager::TypeCount(const Deck::Vector& cards, uint32_t type) {
	int count = 0;
	for(const auto& card : cards) {
		if(card->type & type)
			count++;
	}
	return count;
}
int DeckManager::CountLegends(const Deck::Vector& cards, uint32_t type) {
	int count = 0;
	for(const auto& card : cards) {
		if((card->ot & SCOPE_LEGEND) && (card->type & type))
			count++;
	}
	return count;

}
static DeckError CheckCards(const Deck::Vector& cards, LFList const* curlist,
					  DuelAllowedCards allowedCards,
					  banlist_content_t& ccount,
					  DeckError(*additionalCheck)(const CardDataC*) = nullptr) {
	DeckError ret{ DeckError::NONE };
	for (const auto cit : cards) {
		ret.code = cit->code;
		switch (allowedCards) {
#define CHECK_UNOFFICIAL(cit) if (cit->ot > 0x3) return ret.type = DeckError::UNOFFICIALCARD, ret;
		case DuelAllowedCards::ALLOWED_CARDS_OCG_ONLY:
			CHECK_UNOFFICIAL(cit);
			if (!(cit->ot & 0x1))
				return ret.type = DeckError::TCGONLY, ret;
			break;
		case DuelAllowedCards::ALLOWED_CARDS_TCG_ONLY:
			CHECK_UNOFFICIAL(cit);
			if (!(cit->ot & 0x2))
				return ret.type = DeckError::OCGONLY, ret;
			break;
		case DuelAllowedCards::ALLOWED_CARDS_OCG_TCG:
			CHECK_UNOFFICIAL(cit);
			break;
#undef CHECK_UNOFFICIAL
		case DuelAllowedCards::ALLOWED_CARDS_WITH_PRERELEASE:
			if (cit->ot & 0x1 || cit->ot & 0x2 || cit->ot & 0x100)
				break;
			return ret.type = DeckError::UNOFFICIALCARD, ret;
		case DuelAllowedCards::ALLOWED_CARDS_ANY:
		default:
			break;
		}
		DeckError additional = additionalCheck ? additionalCheck(cit) : DeckError{ DeckError::NONE };
		if (additional.type) {
			return additional;
		}
		uint32_t code = cit->alias ? cit->alias : cit->code;
		ccount[code]++;
		int dc = ccount[code];
		if (dc > 3)
			return ret.type = DeckError::CARDCOUNT, ret;
		auto it = curlist->GetLimitationIterator(cit);
		auto is_end = it == curlist->content.end();
		if ((!is_end && dc > it->second) || (curlist->whitelist && is_end))
			return ret.type = DeckError::LFLIST, ret;
	}
	return { DeckError::NONE };
}
DeckError DeckManager::CheckDeckContent(const Deck& deck, LFList const* lflist, DuelAllowedCards allowedCards, uint32_t forbiddentypes) {
	DeckError ret{ DeckError::NONE };
	if(TypeCount(deck.main, forbiddentypes) > 0 || TypeCount(deck.extra, forbiddentypes) > 0 || TypeCount(deck.side, forbiddentypes) > 0)
		return ret.type = DeckError::FORBTYPE, ret;
	if((CountLegends(deck.main, TYPE_MONSTER) + CountLegends(deck.extra, TYPE_MONSTER)) > 1)
		return ret.type = DeckError::TOOMANYLEGENDS, ret;
	if(CountLegends(deck.main, TYPE_SPELL) > 1)
		return ret.type = DeckError::TOOMANYLEGENDS, ret;
	if(CountLegends(deck.main, TYPE_TRAP) > 1)
		return ret.type = DeckError::TOOMANYLEGENDS, ret;
	if(TypeCount(deck.main, TYPE_SKILL) > 1)
		return ret.type = DeckError::TOOMANYSKILLS, ret;
	banlist_content_t ccount;
	if(!lflist)
		return ret;
	ret = CheckCards(deck.main, lflist, allowedCards, ccount, [](const CardDataC* cit)->DeckError {
		if ((cit->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) || (cit->type & TYPE_LINK && cit->type & TYPE_MONSTER))
			return { DeckError::EXTRACOUNT };
		return { DeckError::NONE };
	});
	if (ret.type) return ret;
	ret = CheckCards(deck.extra, lflist, allowedCards , ccount, [](const CardDataC* cit)->DeckError {
		if (!(cit->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) && !(cit->type & TYPE_LINK && cit->type & TYPE_MONSTER))
			return { DeckError::EXTRACOUNT };
		return { DeckError::NONE };
	});
	if (ret.type) return ret;
	return CheckCards(deck.side, lflist, allowedCards, ccount);
}
DeckError DeckManager::CheckDeckSize(const Deck& deck, const DeckSizes& sizes) {
	DeckError ret{ DeckError::NONE };
	auto skills = TypeCount(deck.main, TYPE_SKILL);
	if(sizes.main != (deck.main.size() - skills)) {
		ret.type = DeckError::MAINCOUNT;
		ret.count.current = static_cast<uint32_t>(deck.main.size()) - skills;
		ret.count.minimum = sizes.main.min;
		ret.count.maximum = sizes.main.max;
	} else if(sizes.extra != deck.extra.size()) {
		ret.type = DeckError::EXTRACOUNT;
		ret.count.current = static_cast<uint32_t>(deck.extra.size());
		ret.count.minimum = sizes.extra.min;
		ret.count.maximum = sizes.extra.max;
	} else if(sizes.side != deck.side.size()) {
		ret.type = DeckError::SIDECOUNT;
		ret.count.current = static_cast<uint32_t>(deck.side.size());
		ret.count.minimum = sizes.side.min;
		ret.count.maximum = sizes.side.max;
	}
	return ret;
}
uint32_t DeckManager::LoadDeckFromBuffer(Deck& deck, uint32_t* dbuf, uint32_t mainc, uint32_t sidec) {
	cardlist_type mainvect(mainc);
	cardlist_type sidevect(sidec);
	auto copy = [&dbuf](uint32_t* vec, uint32_t count) {
		if(count > 0) {
			memcpy(vec, dbuf, count * sizeof(uint32_t));
			dbuf += count;
		}
	};
	copy(mainvect.data(), mainc);
	copy(sidevect.data(), sidec);
	return LoadDeck(deck, mainvect, sidevect);
}
static bool LoadCardList(const epro::path_string& name, cardlist_type* mainlist = nullptr, cardlist_type* extralist = nullptr, cardlist_type* sidelist = nullptr, uint32_t* retmainc = nullptr, uint32_t* retsidec = nullptr) {
	FileStream deck{ name, FileStream::in };
	if(deck.fail())
		return false;
	cardlist_type res;
	std::string str;
	bool is_side = false;
	bool is_extra = false;
	uint32_t sidec = 0;
	while(std::getline(deck, str)) {
		auto pos = str.find_first_of("\n\r");
		if(str.size() && pos != std::string::npos)
			str.erase(pos);
		if(str.empty())
			continue;
		if(str[0] == '#') {
			if(!extralist || str != "#extra")
				continue;
			is_extra = true;
		}
		if(str[0] == '!') {
			is_side = true;
			continue;
		}
		if(str.find_first_of("0123456789") != std::string::npos) {
			uint32_t code = 0;
			try { code = static_cast<uint32_t>(std::stoul(str)); }
			catch (...) { continue; }
			res.push_back(code);
			if(is_side) {
				if(sidelist)
					sidelist->push_back(code);
				sidec++;
			} else {
				if(mainlist && !is_extra)
					mainlist->push_back(code);
				if(extralist && is_extra)
					extralist->push_back(code);
			}
		}
	}
	if(retmainc)
		*retmainc = static_cast<uint32_t>(res.size() - sidec);
	if(retsidec)
		*retsidec = sidec;
	return true;
}
bool DeckManager::LoadDeckFromFile(epro::path_stringview file, Deck& out, bool separated) {
	cardlist_type mainlist;
	cardlist_type sidelist;
	cardlist_type extralist;
	if(!LoadCardList(epro::format(EPRO_TEXT("./deck/{}.ydk"), file), &mainlist, separated ? &extralist : nullptr, &sidelist)) {
		if(!LoadCardList({ file.data(), file.size() }, &mainlist, separated ? &extralist : nullptr, &sidelist))
			return false;
	}
	LoadDeck(out, mainlist, sidelist, separated ? &extralist : nullptr);
	return true;
}
uint32_t DeckManager::LoadDeck(Deck& deck, const cardlist_type& mainlist, const cardlist_type& sidelist, const cardlist_type* extralist) {
	deck.clear();
	uint32_t errorcode = 0;
	const CardDataC* cd = nullptr;
	const bool loadalways = !!extralist;
	for(auto code : mainlist) {
		if(!(cd = gDataManager->GetCardData(code))) {
			cd = gdeckManager->GetDummyOrMappedCardData(code);
			if((!cd || cd->code == 0) && !loadalways) {
				errorcode = code;
				continue;
			}
		}
		if(!cd || cd->type & TYPE_TOKEN)
			continue;
		else if((!extralist || cd->code != 0) && (cd->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ) || (cd->type & TYPE_LINK && cd->type & TYPE_MONSTER))) {
			deck.extra.push_back(cd);
		} else {
			deck.main.push_back(cd);
		}
	}
	if(extralist) {
		for(auto code : *extralist) {
			if(!(cd = gDataManager->GetCardData(code))) {
				cd = gdeckManager->GetDummyOrMappedCardData(code);
				if((!cd || cd->code == 0) && !loadalways) {
					errorcode = code;
					continue;
				}
			}
			if(!cd || cd->type & TYPE_TOKEN)
				continue;
			deck.extra.push_back(cd);
		}
	}
	for(auto code : sidelist) {
		if(!(cd = gDataManager->GetCardData(code))) {
			cd = gdeckManager->GetDummyOrMappedCardData(code);
			if((!cd || cd->code == 0) && !loadalways) {
				errorcode = code;
				continue;
			}
		}
		if(!cd || cd->type & TYPE_TOKEN)
			continue;
		deck.side.push_back(cd);
	}
	return errorcode;
}
bool DeckManager::LoadSide(Deck& deck, uint32_t* dbuf, uint32_t mainc, uint32_t sidec) {
	std::map<uint32_t, int> pcount;
	std::map<uint32_t, int> ncount;
	for(auto& card: deck.main)
		pcount[card->code]++;
	for(auto& card : deck.extra)
		pcount[card->code]++;
	for(auto& card : deck.side)
		pcount[card->code]++;
	auto old_skills = TypeCount(deck.main, TYPE_SKILL);
	auto old_legends_monster = CountLegends(deck.main, TYPE_MONSTER) + CountLegends(deck.extra, TYPE_MONSTER);
	auto old_legends_spell = CountLegends(deck.main, TYPE_SPELL);
	auto old_legends_trap = CountLegends(deck.main, TYPE_TRAP);
	Deck ndeck;
	LoadDeckFromBuffer(ndeck, dbuf, mainc, sidec);
	auto new_skills = TypeCount(ndeck.main, TYPE_SKILL);
	auto new_legends_monster = CountLegends(ndeck.main, TYPE_MONSTER) + CountLegends(ndeck.extra, TYPE_MONSTER);
	if(new_legends_monster > std::max(old_legends_monster, 1))
		return false;
	auto new_legends_spell = CountLegends(ndeck.main, TYPE_SPELL);
	if(new_legends_spell > std::max(old_legends_spell, 1))
		return false;
	auto new_legends_trap = CountLegends(ndeck.main, TYPE_TRAP);
	if(new_legends_trap > std::max(old_legends_trap, 1))
		return false;
	// ideally the check should be only new_skills > 1, but the player might host with don't check deck
	// and thus have more than 1 skill in the deck, do this check to ensure that the sided deck will
	// always be valid in such case and prevent softlocking during side decking
	if(new_skills > std::max(old_skills, 1))
		return false;
	if((ndeck.main.size() - new_skills) != (deck.main.size() - old_skills) || ndeck.extra.size() != deck.extra.size())
		return false;
	for(auto& card : ndeck.main)
		ncount[card->code]++;
	for(auto& card : ndeck.extra)
		ncount[card->code]++;
	for(auto& card : ndeck.side)
		ncount[card->code]++;
	if(!std::equal(pcount.begin(), pcount.end(), ncount.begin()))
		return false;
	deck = ndeck;
	return true;
}
bool DeckManager::SaveDeck(epro::path_stringview name, const Deck& deck) {
	const auto fullname = epro::format(EPRO_TEXT("./deck/{}.ydk"), name);
	FileStream deckfile{ fullname, FileStream::out };
	if(deckfile.fail())
		return false;
	deckfile << "#created by " << BufferIO::EncodeUTF8(mainGame->ebNickName->getText()) << "\n#main\n";
	for(auto card : deck.main)
		deckfile << fmt::to_string(card->code) << "\n";
	deckfile << "#extra\n";
	for(auto card : deck.extra)
		deckfile << fmt::to_string(card->code) << "\n";
	deckfile << "!side\n";
	for(auto card : deck.side)
		deckfile << fmt::to_string(card->code) << "\n";
	return true;
}
bool DeckManager::SaveDeck(epro::path_stringview name, const cardlist_type& mainlist, const cardlist_type& extralist, const cardlist_type& sidelist) {
	const auto fullname = epro::format(EPRO_TEXT("./deck/{}.ydk"), name);
	FileStream deckfile{ fullname, FileStream::out };
	if(deckfile.fail())
		return false;
	deckfile << "#created by " << BufferIO::EncodeUTF8(mainGame->ebNickName->getText()) << "\n#main\n";
	for(auto card : mainlist)
		deckfile << fmt::to_string(card) << "\n";
	deckfile << "#extra\n";
	for(auto card : extralist)
		deckfile << fmt::to_string(card) << "\n";
	deckfile << "!side\n";
	for(auto card : sidelist)
		deckfile << fmt::to_string(card) << "\n";
	return true;
}
const wchar_t* DeckManager::ExportDeckBase64(const Deck& deck) {
	static std::wstring res;
	auto decktobuf = [](const auto& src) {
		static cardlist_type cards;
		cards.resize(src.size());
		for(size_t i = 0; i < src.size(); i++) {
			cards[i] = src[i]->code;
		}
		return base64_encode((uint8_t*)cards.data(), cards.size() * sizeof(cardlist_type::value_type));
	};
	res = epro::format(L"ydke://{}!{}!{}!", decktobuf(deck.main), decktobuf(deck.extra), decktobuf(deck.side));
	return res.data();
}
const wchar_t* DeckManager::ExportDeckCardNames(Deck deck) {
	static std::wstring res;
	res.clear();
	std::sort(deck.main.begin(), deck.main.end(), DataManager::deck_sort_lv);
	std::sort(deck.extra.begin(), deck.extra.end(), DataManager::deck_sort_lv);
	std::sort(deck.side.begin(), deck.side.end(), DataManager::deck_sort_lv);
	auto serialize = [](const auto& list) {
		uint32_t prev = 0;
		uint32_t count = 0;
		for(const auto& card : list) {
			auto code = card->code;
			if(card->alias && abs((int)(card->alias - card->code)) < 10) {
				code = card->alias;
			}
			if(!prev) {
				prev = code;
				count = 1;
			} else if(prev && code != prev) {
				res.append(epro::format(L"{} {}\n", count, gDataManager->GetName(prev)));
				count = 1;
				prev = code;
			} else {
				count++;
			}
		}
		if(prev)
			res.append(epro::format(L"{} {}\n", count, gDataManager->GetName(prev)));
	};
	bool prev = false;
	if(deck.main.size()) {
		res.append(L"Main Deck:\n");
		serialize(deck.main);
		prev = true;
	}
	if(deck.extra.size()) {
		if(prev)
			res.append(L"\n\n");
		res.append(L"Extra Deck:\n");
		serialize(deck.extra);
		prev = true;
	}
	if(deck.side.size()) {
		if(prev)
			res.append(L"\n\n");
		res.append(L"Side Deck:\n");
		serialize(deck.side);
	}
	return res.data();
}
static cardlist_type BufferToCardlist(const std::vector<uint8_t>& input) {
	cardlist_type vect(input.size() / 4);
	memcpy(vect.data(), input.data(), input.size());
	return vect;
}
void DeckManager::ImportDeckBase64(Deck& deck, const wchar_t* buffer) {
	buffer += (sizeof(L"ydke://") / sizeof(wchar_t)) - 1;
	size_t delimiters[3];
	int delim = 0;
	for(int i = 0; delim < 3 && buffer[i]; i++) {
		if(buffer[i] == L'!') {
			delimiters[delim++] = i;
		}
	}
	if(delim != 3)
		return;
	const auto mainlist = BufferToCardlist(base64_decode(buffer, delimiters[0]));
	const auto extralist = BufferToCardlist(base64_decode(buffer + delimiters[0] + 1, delimiters[1] - delimiters[0]));
	const auto sidelist = BufferToCardlist(base64_decode(buffer + delimiters[1] + 1, delimiters[2] - delimiters[1]));
	LoadDeck(deck, mainlist, sidelist, &extralist);
}
template<size_t N>
uint32_t gzinflate(const std::vector<uint8_t>& in, uint8_t(&buffer)[N]) {
	if(in.empty())
		return 0;
	z_stream z{};

	if(inflateInit2(&z, -MAX_WBITS) != Z_OK)
		return 0;

	z.next_in = (decltype(z.next_in))in.data();
	z.avail_in = static_cast<decltype(z.avail_in)>(in.size());

	z.next_out = buffer;
	z.avail_out = N;

	if(inflate(&z, Z_SYNC_FLUSH) < 0 || inflateEnd(&z) != Z_OK)
		return 0;

	return N - z.avail_out;
}

static constexpr size_t BufferSize(size_t mainc, size_t sidec) {
	return (2 * sizeof(uint8_t)) + ((mainc + sidec) * sizeof(uint32_t));
}

bool DeckManager::ImportDeckBase64Omega(Deck& deck, epro::wstringview buffer) {
	constexpr size_t max_main = 60 + 15;
	constexpr size_t max_side = 15;
	constexpr size_t max_size = BufferSize(max_main, max_side);
	uint8_t out_buf[max_size];
	const auto size = gzinflate(base64_decode(buffer, false, true), out_buf);
	if(size < 6) //counts and at least 1 card
		return false;
	const uint8_t mainc = out_buf[0];
	if(mainc > max_main)
		return false;
	const uint8_t sidec = out_buf[1];
	if(sidec > max_side)
		return false;
	if(size < BufferSize(mainc, sidec))
		return false;
	LoadDeckFromBuffer(deck, reinterpret_cast<uint32_t*>(out_buf + 2), mainc, sidec);
	return true;
}
bool DeckManager::DeleteDeck(Deck& deck, epro::path_stringview name) {
	(void)deck;
	return Utils::FileDelete(epro::format(EPRO_TEXT("./deck/{}.ydk"), name));
}
bool DeckManager::RenameDeck(epro::path_stringview oldname, epro::path_stringview newname) {
	return Utils::FileMove(epro::format(EPRO_TEXT("./deck/{}.ydk"), oldname), epro::format(EPRO_TEXT("./deck/{}.ydk"), newname));
}
}
