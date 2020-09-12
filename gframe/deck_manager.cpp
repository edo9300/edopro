#include <algorithm>
#include <fstream>
#include <fmt/format.h>
#include "network.h"
#include "deck_manager.h"
#include "data_manager.h"
#include "game.h"
#include <IGUIEditBox.h>
#include "Base64.h"
#include "utils.h"

namespace ygo {

bool DeckManager::LoadLFListSingle(const path_string& path) {
	bool loaded = false;
	std::ifstream infile(path, std::ifstream::in);
	if(!infile.is_open())
		return loaded;
	LFList lflist;
	lflist.hash = 0;
	std::string str;
	while(std::getline(infile, str)) {
		auto pos = str.find_first_of("\n\r");
		if(str.size() && pos != std::string::npos)
			str = str.substr(0, pos);
		if(str.empty() || str[0] == '#')
			continue;
		if(str[0] == '!') {
			if(lflist.hash)
				_lfList.push_back(lflist);
			lflist.listName = BufferIO::DecodeUTF8s(str.substr(1));
			lflist.content.clear();
			lflist.hash = 0x7dfcee6a;
			lflist.whitelist = false;
			loaded = true;
			continue;
		}
		const std::string key("$whitelist");
		if(str.substr(0, key.size()) == key) {
			lflist.whitelist = true;
		}
		if(!lflist.hash)
			continue;
		pos = str.find(" ");
		if(pos == std::string::npos)
			continue;
		uint32_t code = 0;
		try { code = std::stoul(str.substr(0, pos)); }
		catch(...){}
		if(!code)
			continue;
		str = str.substr(pos + 1);
		str.erase(0, str.find_first_not_of(" \t\n\r\f\v"));
		pos = str.find(" ");
		if(pos == std::string::npos)
			continue;
		int count = 0;
		try { count = std::stoi(str.substr(0, pos)); }
		catch(...) { continue; }
		lflist.content[code] = count;
		lflist.hash = lflist.hash ^ ((code << 18) | (code >> 14)) ^ ((code << (27 + count)) | (code >> (5 - count)));
	}
	if(lflist.hash)
		_lfList.push_back(lflist);
	infile.close();
	return loaded;
}
bool DeckManager::LoadLFListFolder(path_string path) {
	path = Utils::NormalizePath(path);
	bool loaded = false;
	auto lflists = Utils::FindFiles(path, std::vector<path_string>({ EPRO_TEXT("conf") }));
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
	if(null_lflist_index != -1 && null_lflist_index != _lfList.size() -1) {
		auto it = _lfList.begin() + null_lflist_index;
		std::rotate(it, it + 1, _lfList.end());
		null_lflist_index = _lfList.size() - 1;
	}
}
LFList* DeckManager::GetLFList(int lfhash) {
	auto it = std::find_if(_lfList.begin(), _lfList.end(), [lfhash](LFList list) {return list.hash == (unsigned int)lfhash; });
	return it != _lfList.end() ? &*it : nullptr;
}
std::wstring DeckManager::GetLFListName(int lfhash) {
	auto lflist = GetLFList(lfhash);
	return lflist ? lflist->listName.c_str() : gDataManager->unknown_string;
}
int DeckManager::TypeCount(std::vector<CardDataC*> cards, int type) {
	int count = 0;
	for(auto card : cards) {
		if(card->type & type)
			count++;
	}
	return count;
}
inline DeckError CheckCards(const std::vector<CardDataC*> &cards, LFList* curlist, std::unordered_map<uint32_t, int>* list,
					  DuelAllowedCards allowedCards,
					  std::unordered_map<int, int> &ccount,
					  DeckError(*additionalCheck)(CardDataC*) = nullptr) {
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
		int code = cit->alias ? cit->alias : cit->code;
		ccount[code]++;
		int dc = ccount[code];
		if (dc > 3)
			return ret.type = DeckError::CARDCOUNT, ret;
		auto it = list->find(cit->code);
		if (it == list->end())
			it = list->find(code);
		if ((it != list->end() && dc > it->second) || (curlist->whitelist && it == list->end()))
			return ret.type = DeckError::LFLIST, ret;
	}
	return { DeckError::NONE };
}
DeckError DeckManager::CheckDeck(Deck& deck, int lfhash, DuelAllowedCards allowedCards, bool doubled, int forbiddentypes) {
	std::unordered_map<int, int> ccount;
	LFList* curlist = nullptr;
	for(auto& list : _lfList) {
		if(list.hash == (unsigned int)lfhash) {
			curlist = &list;
			break;
		}
	}
	DeckError ret{ DeckError::NONE };
	if(!curlist)
		return ret;
	auto list = &curlist->content;
	if(TypeCount(deck.main, forbiddentypes) > 0 || TypeCount(deck.extra, forbiddentypes) > 0 || TypeCount(deck.side, forbiddentypes) > 0)
		return ret.type = DeckError::FORBTYPE, ret;
	bool speed = mainGame->extra_rules & DECK_LIMIT_20;
	size_t minmain = 40, maxmain = 60, maxextra = 15, maxside = 15;
	if(doubled){
		if(speed){
			maxextra = 10;
			maxside = 12;
		} else {
			minmain = maxmain = 100;
			maxextra = 30;
			maxside = 30;
		}
	} else {
		if(speed){
			minmain = 20;
			maxmain = 30;
			maxextra = 5;
			maxside = 6;
		}
	}
	if(deck.main.size() < minmain || deck.main.size() > maxmain) {
		ret.type = DeckError::MAINCOUNT;
		ret.count.current = deck.main.size();
		ret.count.minimum = minmain;
		ret.count.maximum = maxmain;
	} else if(deck.extra.size() > maxextra) {
		ret.type = DeckError::EXTRACOUNT;
		ret.count.current = deck.extra.size();
		ret.count.minimum = 0;
		ret.count.maximum = maxextra;
	} else if(deck.side.size() > maxside) {
		ret.type = DeckError::SIDECOUNT;
		ret.count.current = deck.side.size();
		ret.count.minimum = 0;
		ret.count.maximum = maxside;
	}
	if(ret.type)
		return ret;
	ret = CheckCards(deck.main, curlist, list, allowedCards, ccount, [](CardDataC* cit)->DeckError {
		if ((cit->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) || (cit->type & TYPE_LINK && cit->type & TYPE_MONSTER))
			return { DeckError::EXTRACOUNT };
		return { DeckError::NONE };
	});
	if (ret.type) return ret;
	ret = CheckCards(deck.extra, curlist, list, allowedCards , ccount, [](CardDataC* cit)->DeckError {
		if (!(cit->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ)) && !(cit->type & TYPE_LINK && cit->type & TYPE_MONSTER))
			return { DeckError::EXTRACOUNT };
		return { DeckError::NONE };
	});
	if (ret.type) return ret;
	return CheckCards(deck.side, curlist, list, allowedCards, ccount);
}
int DeckManager::LoadDeck(Deck& deck, int* dbuf, int mainc, int sidec, int mainc2, int sidec2) {
	std::vector<int> mainvect(mainc + mainc2);
	std::vector<int> sidevect(sidec + sidec2);
	auto copy = [&dbuf](int* vec, int count) {
		memcpy(vec, dbuf, count * sizeof(int));
		dbuf += count;
	};
	copy(&mainvect[0], mainc);
	copy(&sidevect[0], sidec);
	copy(&mainvect[mainc], mainc2);
	copy(&sidevect[sidec], sidec2);
	return LoadDeck(deck, mainvect, sidevect);
}
int DeckManager::LoadDeck(Deck& deck, std::vector<int> mainlist, std::vector<int> sidelist) {
	deck.clear();
	int errorcode = 0;
	CardDataC* cd = nullptr;
	for(auto code : mainlist) {
		if(!(cd = gDataManager->GetCardData(code))) {
			errorcode = code;
			continue;
		}
		if(cd->type & TYPE_TOKEN)
			continue;
		else if((cd->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ) || (cd->type & TYPE_LINK && cd->type & TYPE_MONSTER))) {
			deck.extra.push_back(cd);
		} else {
			deck.main.push_back(cd);
		}
	}
	for(auto code : sidelist) {
		if(!(cd = gDataManager->GetCardData(code))) {
			errorcode = code;
			continue;
		}
		if(cd->type & TYPE_TOKEN)
			continue;
		deck.side.push_back(cd);
	}
	return errorcode;
}
bool LoadCardList(const path_string& name, std::vector<int>* mainlist = nullptr, std::vector<int>* sidelist = nullptr, int* retmainc = nullptr, int* retsidec = nullptr) {
	std::ifstream deck(name, std::ifstream::in);
	if(!deck.is_open())
		return false;
	std::vector<int> res;
	std::string str;
	bool is_side = false;
	int sidec = 0;
	while(std::getline(deck, str)) {
		auto pos = str.find_first_of("\n\r");
		if(str.size() && pos != std::string::npos)
			str = str.substr(0, pos);
		if(str.empty() || str[0] == '#')
			continue;
		if(str[0] == '!') {
			is_side = true;
			continue;
		}
		if(str.find_first_of("0123456789") != std::string::npos) {
			int code = 0;
			try {
				code = std::stoul(str);
			} catch (...){
				continue;
			}
			res.push_back(code);
			if(is_side) {
				if(sidelist)
					sidelist->push_back(code);
				sidec++;
			} else {
				if(mainlist)
					mainlist->push_back(code);
			}
		}
	}
	deck.close();
	if(retmainc)
		*retmainc = res.size() - sidec;
	if(retsidec)
		*retsidec = sidec;
	return true;
}
bool DeckManager::LoadSide(Deck& deck, int* dbuf, int mainc, int sidec) {
	std::map<int, int> pcount;
	std::map<int, int> ncount;
	for(auto& card: deck.main)
		pcount[card->code]++;
	for(auto& card : deck.extra)
		pcount[card->code]++;
	for(auto& card : deck.side)
		pcount[card->code]++;
	Deck ndeck;
	LoadDeck(ndeck, dbuf, mainc, sidec);
	if(ndeck.main.size() != deck.main.size() || ndeck.extra.size() != deck.extra.size())
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
bool DeckManager::LoadDeck(const path_string& file, Deck* deck) {
	std::vector<int> mainlist;
	std::vector<int> sidelist;
	if(!LoadCardList(fmt::format(EPRO_TEXT("./deck/{}.ydk"), file.c_str()), &mainlist, &sidelist)) {
		if(!LoadCardList(file, &mainlist, &sidelist))
			return false;
	}
	if(deck)
		LoadDeck(*deck, mainlist, sidelist);
	else
		LoadDeck(current_deck, mainlist, sidelist);
	return true;
}
bool DeckManager::LoadDeckDouble(const path_string& file, const path_string& file2, Deck* deck) {
	std::vector<int> mainlist;
	std::vector<int> sidelist;
	LoadCardList(fmt::format(EPRO_TEXT("./deck/{}.ydk"), file.c_str()), &mainlist, &sidelist);
	LoadCardList(fmt::format(EPRO_TEXT("./deck/{}.ydk"), file2.c_str()), &mainlist, &sidelist);
	if(deck)
		LoadDeck(*deck, mainlist, sidelist);
	else
		LoadDeck(current_deck, mainlist, sidelist);
	return true;
}
bool DeckManager::SaveDeck(Deck& deck, const path_string& name) {
	std::ofstream deckfile(fmt::format(EPRO_TEXT("./deck/{}.ydk"), name.c_str()), std::ofstream::out);
	if(!deckfile.is_open())
		return false;
	deckfile << "#created by " << BufferIO::EncodeUTF8s(mainGame->ebNickName->getText()) << "\n#main\n";
	for(auto card : deck.main)
		deckfile << std::to_string(card->code) << "\n";
	deckfile << "#extra\n";
	for(auto card : deck.extra)
		deckfile << std::to_string(card->code) << "\n";
	deckfile << "!side\n";
	for(auto card : deck.side)
		deckfile << std::to_string(card->code) << "\n";
	deckfile.close();
	return true;
}
bool DeckManager::SaveDeck(const path_string& name, std::vector<int> mainlist, std::vector<int> extralist, std::vector<int> sidelist) {
	std::ofstream deckfile(fmt::format(EPRO_TEXT("./deck/{}.ydk"), name.c_str()), std::ofstream::out);
	if(!deckfile.is_open())
		return false;
	deckfile << "#created by " << BufferIO::EncodeUTF8s(mainGame->ebNickName->getText()) << "\n#main\n";
	for(auto card : mainlist)
		deckfile << std::to_string(card) << "\n";
	deckfile << "#extra\n";
	for(auto card : extralist)
		deckfile << std::to_string(card) << "\n";
	deckfile << "!side\n";
	for(auto card : sidelist)
		deckfile << std::to_string(card) << "\n";
	deckfile.close();
	return true;
}
const wchar_t* DeckManager::ExportDeckBase64(Deck& deck) {
	static std::wstring res;
	auto decktobuf = [&res=res](const auto& src) {
		static std::vector<int> cards;
		cards.resize(src.size());
		for(size_t i = 0; i < src.size(); i++) {
			cards[i] = src[i]->code;
		}
		res += base64_encode((uint8_t*)cards.data(), cards.size() * 4) + L'!';
	};
	res = L"ydke://";
	decktobuf(deck.main);
	decktobuf(deck.extra);
	decktobuf(deck.side);
	return res.c_str();
}
const wchar_t* DeckManager::ExportDeckCardNames(Deck deck) {
	static std::wstring res;
	res.clear();
	std::sort(deck.main.begin(), deck.main.end(), ClientCard::deck_sort_lv);
	std::sort(deck.extra.begin(), deck.extra.end(), ClientCard::deck_sort_lv);
	std::sort(deck.side.begin(), deck.side.end(), ClientCard::deck_sort_lv);
	auto serialize = [&res=res](const auto& list) {
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
				res.append(gDataManager->GetName(prev)).append(L" x").append(fmt::to_wstring(count)).append(L"\n");
				count = 1;
				prev = code;
			} else {
				count++;
			}
		}
		if(prev)
			res.append(gDataManager->GetName(prev)).append(L" x").append(fmt::to_wstring(count)).append(L"\n");
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
	return res.c_str();
}
void DeckManager::ImportDeckBase64(Deck& deck, const wchar_t* buffer) {
	std::vector<uint8_t> deck_data;
	buffer += (sizeof(L"ydke://") / sizeof(wchar_t)) - 1;
	auto buf = buffer;
	size_t delimiters[3];
	int delim = 0;
	for(int i = 0; delim < 3 && buf[i]; i++) {
		if(buf[i] == L'!') {
			delimiters[delim++] = i;
		}
	}
	if(delim != 3)
		return;
	deck_data = base64_decode(buf, delimiters[0]);
	auto tmpbuf = base64_decode(buf + delimiters[0] + 1, delimiters[1] - delimiters[0]);
	deck_data.insert(deck_data.end(), tmpbuf.begin(), tmpbuf.end());
	int mainc = deck_data.size() / 4;
	tmpbuf = base64_decode(buf + delimiters[1] + 1, delimiters[2] - delimiters[1]);
	deck_data.insert(deck_data.end(), tmpbuf.begin(), tmpbuf.end());
	int sidec = (deck_data.size() / 4) - mainc;
	LoadDeck(deck, (int*)deck_data.data(), mainc, sidec);
}
bool DeckManager::DeleteDeck(Deck& deck, const path_string& name) {
	return Utils::FileDelete(fmt::format(EPRO_TEXT("./deck/{}.ydk"), name.c_str()));
}
bool DeckManager::RenameDeck(const path_string& oldname, const path_string& newname) {
	return Utils::FileMove(EPRO_TEXT("./deck/") + oldname + EPRO_TEXT(".ydk"), EPRO_TEXT("./deck/") + newname + EPRO_TEXT(".ydk"));
}
}
