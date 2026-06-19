#include "data_manager.h"
#include <IReadFile.h>
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <stack>
#include "ireadfile_sqlite.h"
#include "bufferio.h"
#include "logging.h"
#include "utils.h"
#include "common.h"
#include "file_stream.h"
#include "fmt.h"

#if !defined(SQLITE_NOTICE)
#define SQLITE_NOTICE      27
#endif
#if !defined(SQLITE_WARNING)
#define SQLITE_WARNING     28
#endif

namespace ygo {

constexpr epro::wstringview DataManager::unknown_string;
static constexpr auto SELECT_STMT =
R"(SELECT datas.id,datas.ot,datas.alias,datas.setcode,datas.type,datas.atk,datas.def,datas.level,datas.race,datas.attribute,datas.category,texts.name,texts.desc,texts.str1,texts.str2,texts.str3,texts.str4,texts.str5,texts.str6,texts.str7,texts.str8,texts.str9,texts.str10,texts.str11,texts.str12,texts.str13,texts.str14,texts.str15,texts.str16
FROM datas,texts WHERE texts.id = datas.id ORDER BY texts.id;)"sv;

static constexpr auto SELECT_STMT_LOCALE =
R"(SELECT id,name,desc,str1,str2,str3,str4,str5,str6,str7,str8,str9,str10,str11,str12,str13,str14,str15,str16
FROM texts ORDER BY texts.id;)"sv;

DataManager::DataManager() : irrvfs(irrsqlite_createfilesystem()) {
	if(sqlite3_threadsafe())
		sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
	sqlite3_initialize();
	sqlite3_vfs_register(irrvfs.get(), 0);
	cards.reserve(25000);
	locales.reserve(15000);
}

DataManager::~DataManager() {
	sqlite3_vfs_unregister(irrvfs.get());
	sqlite3_shutdown();
}

void DataManager::ClearLocaleTexts() {
	for(auto& [code, localized_card_data] : indexes) {
		auto& [card_data, localized_string] = localized_card_data;
		localized_string = nullptr;
		if(card_data)
			card_data->_locale_strings = nullptr;
	}
	locales.clear();
}

sqlite3* DataManager::OpenDb(epro::path_stringview file) {
	cur_database = Utils::ToUTF8IfNeeded(file);
	sqlite3* pDB{ nullptr };
	if(sqlite3_open_v2(Utils::ToUTF8IfNeeded(Utils::GetAbsolutePath(file)).data(), &pDB, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
		Error(pDB);
		pDB = nullptr;
	}
	return pDB;
}

sqlite3* DataManager::OpenDb(irr::io::IReadFile* reader) {
	const auto& filename = reader->getFileName();
	cur_database = Utils::ToUTF8IfNeeded({ filename.data(), filename.size() });
	sqlite3* pDB{ nullptr };
	if(irrdb_open(reader, &pDB, SQLITE_OPEN_READONLY) != SQLITE_OK) {
		Error(pDB);
		pDB = nullptr;
	}
	return pDB;
}

static inline bool GetWstring(std::wstring& out, sqlite3_stmt* stmt, int iCol) {
#if WCHAR_MAX == UINT16_MAX
	auto* text = static_cast<const wchar_t*>(sqlite3_column_text16(stmt, iCol));
	if(text == nullptr || *text == L'\0') {
		out.clear();
		return false;
	}
	auto len = static_cast<size_t>(sqlite3_column_bytes16(stmt, iCol)) / sizeof(wchar_t);
	out.assign(text, len);
#else
	auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, iCol));
	if(text == nullptr || *text == '\0') {
		out.clear();
		return false;
	}
	auto len = static_cast<size_t>(sqlite3_column_bytes(stmt, iCol));
	out = BufferIO::DecodeUTF8({ text, len });
#endif
	return true;
}

bool DataManager::ParseDB(sqlite3* pDB) {
	if(pDB == nullptr)
		return false;
	sqlite3_stmt* pStmt;
	if(sqlite3_prepare_v2(pDB, SELECT_STMT.data(), static_cast<int>(SELECT_STMT.size() + 1), &pStmt, 0) != SQLITE_OK)
		return Error(pDB);
	auto indexesiterator = indexes.begin();
	for(int step = sqlite3_step(pStmt); step != SQLITE_DONE; step = sqlite3_step(pStmt)) {
		if(step == SQLITE_WARNING || step == SQLITE_NOTICE)
			continue;
		if(step != SQLITE_ROW)
			return Error(pDB, pStmt);
		uint32_t code = static_cast<uint32_t>(sqlite3_column_int64(pStmt, 0));
		auto ptr = &cards[code];
		CardString*& localestring = ptr->_locale_strings;
		localestring = nullptr;
		if(indexesiterator != indexes.end()) {
			while(indexesiterator != indexes.end() && indexesiterator->first < code)
				indexesiterator++;
			if(indexesiterator != indexes.end() && indexesiterator->first == code)
				localestring = indexesiterator->second.second;
		}
		CardString& cs = ptr->_strings;
		CardDataC& cd = ptr->_data;
		cd.code = code;
		cd.ot = static_cast<uint32_t>(sqlite3_column_int64(pStmt, 1));
		cd.alias = static_cast<uint32_t>(sqlite3_column_int64(pStmt, 2));
		cd.setcodes_p = nullptr;
		cd.setcodes.clear();
		uint64_t setcodes = sqlite3_column_int64(pStmt, 3);
		for(int i = 0; i < 4; i++) {
			uint16_t setcode = (setcodes >> (i * 16)) & 0xffff;
			if(setcode)
				cd.setcodes.push_back(setcode);
		}
		if(cd.setcodes.size()) {
			cd.setcodes.push_back(0);
			cd.setcodes_p = cd.setcodes.data();
		}
		cd.type = static_cast<uint32_t>(sqlite3_column_int64(pStmt, 4));
		cd.attack = sqlite3_column_int(pStmt, 5);
		cd.defense = sqlite3_column_int(pStmt, 6);
		if(cd.type & TYPE_LINK) {
			cd.link_marker = cd.defense;
			cd.defense = 0;
		} else
			cd.link_marker = 0;

		int level = sqlite3_column_int(pStmt, 7);
		if(level < 0)
			cd.level = -(level & 0xff);
		else
			cd.level = level & 0xff;

		cd.lscale = (level >> 24) & 0xff;
		cd.rscale = (level >> 16) & 0xff;
		cd.race = static_cast<uint64_t>(sqlite3_column_int64(pStmt, 8));
		cd.attribute = static_cast<uint32_t>(sqlite3_column_int64(pStmt, 9));
		cd.category = static_cast<uint32_t>(sqlite3_column_int64(pStmt, 10));

		if(GetWstring(cs.name, pStmt, 11))
			cs.uppercase_name = Utils::ToUpperNoAccents(cs.name);
		else
			cs.uppercase_name.clear();

		if(GetWstring(cs.text, pStmt, 12))
			cs.uppercase_text = Utils::ToUpperNoAccents(cs.text);
		else
			cs.uppercase_text.clear();

		for(int i = 0; i < 16; ++i)
			(void)GetWstring(cs.desc[i], pStmt, i + 13);

		if(localestring)
			indexesiterator->second.first = ptr;
		else
			indexesiterator = indexes.emplace_hint(indexesiterator, cd.code, std::make_pair(ptr, localestring));
	}
	sqlite3_finalize(pStmt);
	sqlite3_close(pDB);
	return true;
}
bool DataManager::ParseLocaleDB(sqlite3* pDB) {
	if(pDB == nullptr)
		return false;
	sqlite3_stmt* pStmt;
	if(sqlite3_prepare_v2(pDB, SELECT_STMT_LOCALE.data(), static_cast<int>(SELECT_STMT_LOCALE.size() + 1), &pStmt, 0) != SQLITE_OK)
		return Error(pDB);
	auto indexesiterator = indexes.begin();
	for(int step = sqlite3_step(pStmt); step != SQLITE_DONE; step = sqlite3_step(pStmt)) {
		if(step == SQLITE_WARNING || step == SQLITE_NOTICE)
			continue;
		if(step != SQLITE_ROW)
			return Error(pDB, pStmt);

		auto code = static_cast<uint32_t>(sqlite3_column_int64(pStmt, 0));

		CardString& cs = locales[code];
		auto ptr = &cs;

		if(GetWstring(cs.name, pStmt, 1))
			cs.uppercase_name = Utils::ToUpperNoAccents(cs.name);
		else
			cs.uppercase_name.clear();

		if(GetWstring(cs.text, pStmt, 2))
			cs.uppercase_text = Utils::ToUpperNoAccents(cs.text);
		else
			cs.uppercase_text.clear();

		for(int i = 0; i < 16; ++i)
			(void)GetWstring(cs.desc[i], pStmt, i + 3);

		CardDataM* card_data = nullptr;
		if(indexesiterator != indexes.end()) {
			while(indexesiterator != indexes.end() && indexesiterator->first < code)
				indexesiterator++;
			if(indexesiterator != indexes.end() && indexesiterator->first == code)
				card_data = indexesiterator->second.first;
		}
		if(card_data) {
			card_data->_locale_strings = ptr;
			indexesiterator->second.second = ptr;
		} else {
			indexesiterator = indexes.emplace_hint(indexesiterator, code, std::make_pair(card_data, ptr));
		}
	}
	sqlite3_finalize(pStmt);
	sqlite3_close(pDB);
	return true;
}
bool DataManager::LoadStrings(const epro::path_string& file) {
	FileStream string_file{ file, FileStream::in };
	if(string_file.fail())
		return false;
	std::string str;
	while(std::getline(string_file, str)) {
		auto pos = str.find('\r');
		if(str.size() && pos != std::string::npos)
			str.erase(pos);
		if(str.empty() || str.at(0) != '!') {
			continue;
		}
		pos = str.find(' ');
		auto type = str.substr(1, pos - 1);
		str.erase(0, pos + 1);
		pos = str.find(' ');
		auto value = str.substr(0, pos);
		str.erase(0, pos + 1);
		try {
			if(type == "system") {
				_sysStrings.SetMain(std::stoi(value), BufferIO::DecodeUTF8(str));
			} else {
				LocaleStringHelper* obj;
				if(type == "victory")
					obj = &_victoryStrings;
				else if(type == "counter")
					obj = &_counterStrings;
				else if(type == "setname")
					obj = &_setnameStrings;
				else
					continue;
				obj->SetMain(std::stoi(value, 0, 16), BufferIO::DecodeUTF8(str));
			}
		}
		catch(...) {}
	}
	return true;
}
bool DataManager::LoadLocaleStrings(const epro::path_string& file) {
	FileStream string_file{ file, FileStream::in };
	if(string_file.fail())
		return false;
	std::string str;
	while(std::getline(string_file, str)) {
		auto pos = str.find('\r');
		if(str.size() && pos != std::string::npos)
			str.erase(pos);
		if(str.empty() || str.at(0) != '!') {
			continue;
		}
		pos = str.find(' ');
		auto type = str.substr(1, pos - 1);
		str.erase(0, pos + 1);
		pos = str.find(' ');
		auto value = str.substr(0, pos);
		str.erase(0, pos + 1);
		try {
			if(type == "system") {
				_sysStrings.SetLocale(std::stoi(value), BufferIO::DecodeUTF8(str));
			} else {
				LocaleStringHelper* obj;
				if(type == "victory")
					obj = &_victoryStrings;
				else if(type == "counter")
					obj = &_counterStrings;
				else if(type == "setname")
					obj = &_setnameStrings;
				else
					continue;
				obj->SetLocale(std::stoi(value, 0, 16), BufferIO::DecodeUTF8(str));
			}
		}
		catch(...) {}
	}
	return true;
}
bool DataManager::LoadIdsMapping(const epro::path_string& file) {
	FileStream mappings_file{ file, FileStream::in };
	if(mappings_file.fail())
		return false;
	nlohmann::json mappings;
	try {
		mappings_file >> mappings;
	} catch(const std::exception& e) {
		ErrorLog("Failed to load id mappings json \"{}\": {}", Utils::ToUTF8IfNeeded(file), e.what());
		return false;
	}
	auto cit = mappings.find("mappings");
	if(cit == mappings.end() || !cit->is_array())
		return false;
	try {
		for(auto& obj : *cit) {
			auto [old_code, new_code] = obj.get<std::pair<uint32_t, uint32_t>>();
			mapped_ids[old_code] = new_code;
		}
	} catch(const std::exception& e) {
		ErrorLog("Error while parsing mappings json \"{}\": {}", Utils::ToUTF8IfNeeded(file), e.what());
		return false;
	};
	return true;
}
void DataManager::ClearLocaleStrings() {
	_sysStrings.ClearLocales();
	_victoryStrings.ClearLocales();
	_counterStrings.ClearLocales();
	_setnameStrings.ClearLocales();
}
bool DataManager::Error(sqlite3* pDB, sqlite3_stmt* pStmt) const {
	ErrorLog("Error when loading database ({}): {}", cur_database, sqlite3_errmsg(pDB));
	if(pStmt)
		sqlite3_finalize(pStmt);
	sqlite3_close(pDB);
	return false;
}
const CardDataC* DataManager::GetCardData(uint32_t code) const {
	auto it = cards.find(code);
	if(it != cards.end())
		return &it->second._data;
	return nullptr;
}
const CardDataC* DataManager::GetMappedCardData(uint32_t code) const {
	auto it = mapped_ids.find(code);
	if(it != mapped_ids.end())
		return gDataManager->GetCardData(it->second);
	return nullptr;
}
epro::wstringview DataManager::GetName(uint32_t code) const {
	auto csit = cards.find(code);
	if(csit == cards.end() || csit->second.GetStrings().name.empty())
		return unknown_string;
	return csit->second.GetStrings().name;
}
epro::wstringview DataManager::GetText(uint32_t code) const {
	auto csit = cards.find(code);
	if(csit == cards.end() || csit->second.GetStrings().text.empty())
		return unknown_string;
	return csit->second.GetStrings().text;
}
epro::wstringview DataManager::GetUppercaseName(uint32_t code) const {
	auto csit = cards.find(code);
	if(csit == cards.end() || csit->second.GetStrings().name.empty())
		return unknown_string;
	return csit->second.GetStrings().uppercase_name;
}
epro::wstringview DataManager::GetUppercaseText(uint32_t code) const {
	auto csit = cards.find(code);
	if(csit == cards.end() || csit->second.GetStrings().text.empty())
		return unknown_string;
	return csit->second.GetStrings().uppercase_text;
}
epro::wstringview DataManager::GetDesc(uint64_t strCode, bool compat) const {
	uint32_t code = 0;
	uint32_t stringid = 0;
	if(compat) {
		if(strCode < 10000)
			return GetSysString(strCode);
		code = static_cast<uint32_t>(strCode >> 4);
		stringid = static_cast<uint32_t>(strCode & 0xf);
	} else {
		code = static_cast<uint32_t>(strCode >> 20);
		stringid = static_cast<uint32_t>(strCode & 0xfffff);
	}
	if(code == 0)
		return GetSysString(stringid);
	auto csit = cards.find(code);
	if(csit == cards.end())
		return unknown_string;
	epro::wstringview desc = csit->second.GetDesc(stringid);
	if(desc.empty())
		return unknown_string;
	return desc;
}
std::vector<uint16_t> DataManager::GetSetCode(const std::vector<epro::wstringview>& setname) const {
	std::vector<uint16_t> res;
	for(const auto& [setcode, localized_name] : _setnameStrings.map) {
		const auto& [base_name, localized] = localized_name;
		if(base_name.empty())
			continue;
		const auto str = Utils::ToUpperNoAccents(localized.size() ? localized : base_name);
		if(str.find(L'|') != std::wstring::npos) {
			for(const auto& name : Utils::TokenizeString<epro::wstringview>(str, L'|')) {
				if(Utils::ContainsSubstring(name, setname)) {
					res.push_back(static_cast<uint16_t>(setcode));
					break;
				}
			}
		} else {
			if(Utils::ContainsSubstring(str, setname))
				res.push_back(static_cast<uint16_t>(setcode));
		}
	}
	return res;
}
std::wstring DataManager::GetNumString(size_t num, bool bracket) const {
	if(!bracket)
		return epro::to_wstring(num);
	return epro::format(L"({})", num);
}

epro::wstringview DataManager::FormatLocation(uint32_t location, int sequence) const {
	if(location == 0x8) {
		if(sequence < 5)
			return GetSysString(1003);
		else if(sequence == 5)
			return GetSysString(1008);
		else
			return GetSysString(1009);
	}
	uint32_t filter = 1;
	int i = 1000;
	for(; filter != 0x100 && filter != location; filter <<= 1)
		++i;
	if(filter == location)
		return GetSysString(i);
	else
		return unknown_string;
}
std::wstring DataManager::FormatAttribute(uint32_t attribute) const {
	std::wstring res;
	for(uint32_t i = 1010, filter = 1; filter <= ATTRIBUTE_DIVINE; filter <<= 1, ++i) {
		if(attribute & filter) {
			if(!res.empty())
				res += L'|';
			res += GetSysString(i);
		}
	}
	if(res.empty())
		return std::wstring{ unknown_string };
	return res;
}
static std::wstring FormatSkill(uint64_t skill_type) {
	std::wstring res;
	for(uint32_t i = 2100; skill_type; skill_type >>= 1, ++i) {
		if(skill_type & 0x1u) {
			if(!res.empty())
				res += L'|';
			res += gDataManager->GetSysString(i);
		}
	}
	if(res.empty())
		return std::wstring{ DataManager::unknown_string };
	return res;
}
std::wstring DataManager::FormatRace(uint64_t race, bool isSkill) const {
	if(isSkill) return FormatSkill(race);
	std::wstring res;
	for(uint32_t i = 0; race; race >>= 1, ++i) {
		if(race & 0x1u) {
			if(!res.empty())
				res += L'|';
			res += GetSysString(GetRaceStringIndex(i));
		}
	}
	if(res.empty())
		return std::wstring{ unknown_string };
	return res;
}
std::wstring DataManager::FormatType(uint32_t type) const {
	std::wstring res;
	if(type & TYPE_SKILL)
		res += GetSysString(1077);
	if(type & TYPE_ACTION) {
		if (!res.empty())
			res += L'|';
		res += GetSysString(1078);
	}
	for(uint32_t i = 1050, filter = 1; filter != TYPE_SKILL; filter <<= 1, ++i) {
		if(type & filter) {
			if(!res.empty())
				res += L'|';
			res += GetSysString(i);
		}
	}
	if(res.empty())
		return std::wstring{ unknown_string };
	return res;
}
std::wstring DataManager::FormatScope(uint32_t scope, bool hideOCGTCG) const {
	static constexpr std::pair<uint32_t, uint32_t> SCOPES[]{
		{SCOPE_OCG, 1900},
		{SCOPE_TCG, 1901},
		{SCOPE_ANIME, 1265},
		{SCOPE_ILLEGAL, 1266},
		{SCOPE_VIDEO_GAME, 1267},
		{SCOPE_CUSTOM, 1268},
		{SCOPE_SPEED, 1910},
		{SCOPE_PRERELEASE, 1903},
		{SCOPE_RUSH, 1911}
	};
	if (hideOCGTCG && scope == SCOPE_OCG_TCG) return L"";
	std::wstring buffer;
	for(const auto [val, stringid] : SCOPES) {
		if (scope & val) {
			if (!buffer.empty()) {
				buffer += L'/';
			}
			buffer += GetSysString(stringid);
		}
	}
	return buffer;
}
std::wstring DataManager::FormatSetName(const std::vector<uint16_t>& setcodes) const {
	std::wstring res;
	for(auto& setcode : setcodes) {
		if(!setcode)
			break;
		auto name = GetSetName(setcode);
		if(!res.empty())
			res += L'|';
		if(name.empty())
			res += unknown_string;
		else
			res += name;
	}
	return res;
}
std::wstring DataManager::FormatLinkMarker(uint32_t link_marker) const {
	return epro::format(L"{}{}{}{}{}{}{}{}",
					   (link_marker & LINK_MARKER_TOP_LEFT)		? L"[\u2196]" : L"",
					   (link_marker & LINK_MARKER_TOP)			? L"[\u2191]" : L"",
					   (link_marker & LINK_MARKER_TOP_RIGHT)	? L"[\u2197]" : L"",
					   (link_marker & LINK_MARKER_LEFT)			? L"[\u2190]" : L"",
					   (link_marker & LINK_MARKER_RIGHT)		? L"[\u2192]" : L"",
					   (link_marker & LINK_MARKER_BOTTOM_LEFT)	? L"[\u2199]" : L"",
					   (link_marker & LINK_MARKER_BOTTOM)		? L"[\u2193]" : L"",
					   (link_marker & LINK_MARKER_BOTTOM_RIGHT)	? L"[\u2198]" : L"");
}
void DataManager::CardReader(void* payload, uint32_t code, OCG_CardData* data) {
	auto carddata = static_cast<DataManager*>(payload)->GetCardData(code);
	if(carddata != nullptr)
		memcpy(data, carddata, sizeof(CardData));
}
int DataManager::OpcodesAllowDeclaringCards(void* payload, const uint64_t* opcode_list, int opcode_num) {
	for(const auto& [code, cd] : gDataManager->cards) {
		if(IsCardDeclarable(&cd._data, opcode_list, opcode_num, false))
			return 1;
	}
	return 0;
}
#define BINARY_OP(opcode,op) case opcode: {\
								if (stack.size() >= 2) {\
									auto rhs = stack.top();\
									stack.pop();\
									auto lhs = stack.top();\
									stack.pop();\
									stack.push(lhs op rhs);\
								}\
								break;\
							}
#define UNARY_OP(opcode,op) case opcode: {\
								if (stack.size() >= 1) {\
									auto val = stack.top();\
									stack.pop();\
									stack.push(op val);\
								}\
								break;\
							}
#define UNARY_OP_OP(opcode,val,op) UNARY_OP(opcode,cd->val op)
#define GET_OP(opcode,val) case opcode: {\
								stack.push(cd->val);\
								break;\
							}

//special cards
#define CARD_MARINE_DOLPHIN	78734254
#define CARD_TWINKLE_MOSS	13857930

bool DataManager::IsCardDeclarable(const CardDataC* cd, const uint64_t* opcode_list, size_t opcode_num, bool compat_mode) {
	std::stack<int64_t> stack;
	bool alias = false, token = false;
	for(size_t i = 0; i < opcode_num; ++i) {
		auto opcode = opcode_list[i];
		switch(opcode << (compat_mode ? 32 : 0)) {
		BINARY_OP(OPCODE_ADD, +);
		BINARY_OP(OPCODE_SUB, -);
		BINARY_OP(OPCODE_MUL, *);
		BINARY_OP(OPCODE_DIV, /);
		BINARY_OP(OPCODE_AND, &&);
		BINARY_OP(OPCODE_OR, ||);
		UNARY_OP(OPCODE_NEG, -);
		UNARY_OP(OPCODE_NOT, !);
		BINARY_OP(OPCODE_BAND, &);
		BINARY_OP(OPCODE_BOR, |);
		UNARY_OP(OPCODE_BNOT, ~);
		BINARY_OP(OPCODE_BXOR, ^);
		BINARY_OP(OPCODE_LSHIFT, <<);
		BINARY_OP(OPCODE_RSHIFT, >>);
		UNARY_OP_OP(OPCODE_ISCODE, code, ==);
		UNARY_OP_OP(OPCODE_ISTYPE, type, &);
		UNARY_OP_OP(OPCODE_ISRACE, race, &);
		UNARY_OP_OP(OPCODE_ISATTRIBUTE, attribute, &);
		GET_OP(OPCODE_GETCODE, code);
		GET_OP(OPCODE_GETTYPE, type);
		GET_OP(OPCODE_GETRACE, race);
		GET_OP(OPCODE_GETATTRIBUTE, attribute);
		//GET_OP(OPCODE_GETSETCARD, setcode);
		case OPCODE_ISSETCARD: {
			if (stack.size() >= 1) {
				int set_code = stack.top();
				stack.pop();
				bool res = false;
				uint16_t settype = set_code & 0xfff;
				uint16_t setsubtype = set_code & 0xf000;
				for(auto& sc : cd->setcodes) {
					if((sc & 0xfff) == settype && (sc & 0xf000 & setsubtype) == setsubtype) {
						res = true;
						break;
					}
				}
				stack.push(res);
			}
			break;
		}
		case OPCODE_ALLOW_ALIASES: {
			alias = true;
			break;
		}
		case OPCODE_ALLOW_TOKENS: {
			token = true;
			break;
		}
		default: {
			stack.push(opcode);
			break;
		}
		}
	}
	if(stack.size() != 1 || stack.top() == 0)
		return false;
	return cd->code == CARD_MARINE_DOLPHIN || cd->code == CARD_TWINKLE_MOSS
		|| ((alias || !cd->alias) && (token || ((cd->type & (TYPE_MONSTER + TYPE_TOKEN)) != (TYPE_MONSTER + TYPE_TOKEN))));
}
#undef BINARY_OP
#undef UNARY_OP
#undef UNARY_OP_OP
#undef GET_OP

inline bool is_skill(uint32_t type) {
	return (type & (TYPE_SKILL | TYPE_ACTION));
}
inline bool check_both_skills(uint32_t type1, uint32_t type2) {
	return is_skill(type1) && is_skill(type2);
}
inline bool check_either_skills(uint32_t type1, uint32_t type2) {
	return is_skill(type1) || is_skill(type2);
}
static constexpr uint32_t monster_spell_trap = TYPE_MONSTER | TYPE_SPELL | TYPE_TRAP;
static constexpr uint32_t not_monster_spell_trap = ~monster_spell_trap;
static constexpr uint32_t spsummon_proc_types = TYPE_LINK | TYPE_XYZ | TYPE_SYNCHRO | TYPE_RITUAL | TYPE_FUSION;

inline bool check_codes(const CardDataC* p1, const CardDataC* p2) {
	if(p1->alias == p2->code)
		return false;
	if(p2->alias == p1->code)
		return true;
	if(p1->IsInArtworkOffsetRange() && p2->IsInArtworkOffsetRange() && p1->alias == p2->alias) {
		auto inc1 = (p1->code < p1->alias) * 2 * CardDataC::CARD_ARTWORK_VERSIONS_OFFSET;
		auto inc2 = (p2->code < p1->alias) * 2 * CardDataC::CARD_ARTWORK_VERSIONS_OFFSET;
		return p1->code + inc1 < p2->code + inc2;
	}
	return p1->code < p2->code;
}

inline bool check_skills(const CardDataC* p1, const CardDataC* p2) {
	if(check_both_skills(p1->type, p2->type)) {
		if((p1->type & not_monster_spell_trap) == (p2->type & not_monster_spell_trap)) {
			return check_codes(p1, p2);
		} else {
			return (p1->type & not_monster_spell_trap) < (p2->type & not_monster_spell_trap);
		}
	}
	return is_skill(p2->type);
}
static bool card_sorter(const CardDataC* p1, const CardDataC* p2, bool(*sortoop)(const CardDataC* p1, const CardDataC* p2)) {
	if(check_either_skills(p1->type, p2->type))
		return check_skills(p1, p2);
	if((p1->type & monster_spell_trap) != (p2->type & monster_spell_trap))
		return (p1->type & monster_spell_trap) < (p2->type & monster_spell_trap);
	if((p1->type & monster_spell_trap) == TYPE_MONSTER) {
		return sortoop(p1, p2);
	}
	if((p1->type & not_monster_spell_trap) != (p2->type & not_monster_spell_trap))
		return (p1->type & not_monster_spell_trap) < (p2->type & not_monster_spell_trap);
	return check_codes(p1, p2);
}
inline uint32_t get_monster_card_type(uint32_t type) {
	if(type & spsummon_proc_types)
		return type & (spsummon_proc_types | TYPE_MONSTER);
	return type & (TYPE_NORMAL | TYPE_EFFECT | TYPE_MONSTER);
}
bool DataManager::deck_sort_lv(const CardDataC* p1, const CardDataC* p2) {
	return card_sorter(p1, p2, [](const CardDataC* p1, const CardDataC* p2)->bool {
		uint32_t type1 = get_monster_card_type(p1->type);
		uint32_t type2 = get_monster_card_type(p2->type);
		if(type1 != type2)
			return type1 < type2;
		if(p1->level != p2->level)
			return p1->level > p2->level;
		if(p1->attack != p2->attack)
			return p1->attack > p2->attack;
		if(p1->defense != p2->defense)
			return p1->defense > p2->defense;
		return check_codes(p1, p2);
	});
}
bool DataManager::deck_sort_atk(const CardDataC* p1, const CardDataC* p2) {
	return card_sorter(p1, p2, [](const CardDataC* p1, const CardDataC* p2)->bool {
		if(p1->attack != p2->attack)
			return p1->attack > p2->attack;
		if(p1->defense != p2->defense)
			return p1->defense > p2->defense;
		if(p1->level != p2->level)
			return p1->level > p2->level;
		uint32_t type1 = get_monster_card_type(p1->type);
		uint32_t type2 = get_monster_card_type(p2->type);
		if(type1 != type2)
			return type1 < type2;
		return check_codes(p1, p2);
	});
}
bool DataManager::deck_sort_def(const CardDataC* p1, const CardDataC* p2) {
	return card_sorter(p1, p2, [](const CardDataC* p1, const CardDataC* p2)->bool {
		if(p1->defense != p2->defense)
			return p1->defense > p2->defense;
		if(p1->attack != p2->attack)
			return p1->attack > p2->attack;
		if(p1->level != p2->level)
			return p1->level > p2->level;
		uint32_t type1 = get_monster_card_type(p1->type);
		uint32_t type2 = get_monster_card_type(p2->type);
		if(type1 != type2)
			return type1 < type2;
		return check_codes(p1, p2);
	});
}
bool DataManager::deck_sort_name(const CardDataC* p1, const CardDataC* p2) {
	int res = gDataManager->GetUppercaseName(p1->code).compare(gDataManager->GetUppercaseName(p2->code));
	if(res != 0)
		return res < 0;
	return check_codes(p1, p2);
}

}
