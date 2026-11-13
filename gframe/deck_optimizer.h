#ifndef DECK_OPTIMIZER_H
#define DECK_OPTIMIZER_H
// Deck optimiser: lets the player tag what every card in the deck is for
// (starter, extender, hand trap, ...) and turns those tags into opening hand
// odds and plain-language suggestions.
// Header only on purpose: platform builds that list their sources explicitly
// (Android, iOS) pick it up without any change to their build files.

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "common.h"
#include "data_manager.h"
#include "deck.h"
#include "file_stream.h"
#include "fmt.h"
#include "text_types.h"
#include "utils.h"

namespace ygo {
namespace rka {

enum Role : uint8_t {
	ROLE_NONE = 0,
	ROLE_STARTER,
	ROLE_EXTENDER,
	ROLE_ENGINE_REQ,
	ROLE_HANDTRAP,
	ROLE_BREAKER,
	ROLE_BRICK,
	ROLE_TECH,
	ROLE_COUNT
};

inline const wchar_t* RoleName(Role role) {
	switch(role) {
	case ROLE_STARTER: return L"Starter";
	case ROLE_EXTENDER: return L"Extender";
	case ROLE_ENGINE_REQ: return L"Engine requirement";
	case ROLE_HANDTRAP: return L"Hand trap";
	case ROLE_BREAKER: return L"Board breaker";
	case ROLE_BRICK: return L"Brick";
	case ROLE_TECH: return L"Tech / other";
	default: return L"Not tagged";
	}
}

inline const wchar_t* RoleHelp(Role role) {
	switch(role) {
	case ROLE_STARTER: return L"Starts your combo all by itself. You want one in every opening hand.";
	case ROLE_EXTENDER: return L"Keeps the combo going or plays through a negate. Does nothing without a starter.";
	case ROLE_ENGINE_REQ: return L"Has to be in the deck for the combo to work, but you never want to draw it.";
	case ROLE_HANDTRAP: return L"Stops the opponent from your hand during their turn.";
	case ROLE_BREAKER: return L"Clears the opponent's board when you go second.";
	case ROLE_BRICK: return L"Dead card in the opening hand.";
	case ROLE_TECH: return L"Anything else: staples, traps, flex spots.";
	default: return L"Tap a role on the right to say what this card does.";
	}
}

// How the built-in opponent treats a known hand trap.
enum HandtrapKind : uint8_t {
	HT_GENERIC = 0,
	HT_NEGATE_ACTIVATION, // Ash Blossom: answers a search/draw/summon from deck
	HT_NEGATE_MONSTER,    // Effect Veiler, Infinite Impermanence, Ghost Ogre
	HT_BOARD_WIPE,        // Nibiru
	HT_DRAW,              // Maxx "C", Mulcharmy
	HT_FLOODGATE,         // Droll, Shifter, Lancea
	HT_GRAVE,             // D.D. Crow, Ghost Belle, Bystials
};

struct KnownHandtrap {
	uint32_t code;
	HandtrapKind kind;
};

inline const std::vector<KnownHandtrap>& KnownHandtraps() {
	static const std::vector<KnownHandtrap> list{
		{ 14558127, HT_NEGATE_ACTIVATION }, // Ash Blossom & Joyous Spring
		{ 97268402, HT_NEGATE_MONSTER },    // Effect Veiler
		{ 10045474, HT_NEGATE_MONSTER },    // Infinite Impermanence
		{ 27204311, HT_BOARD_WIPE },        // Nibiru, the Primal Being
		{ 94145021, HT_FLOODGATE },         // Droll & Lock Bird
		{ 59438930, HT_NEGATE_MONSTER },    // Ghost Ogre & Snow Rabbit
		{ 73642296, HT_GRAVE },             // Ghost Belle & Haunted Mansion
		{ 23434538, HT_DRAW },              // Maxx "C"
		{ 42141493, HT_DRAW },              // Mulcharmy Fuwalos
		{ 84192580, HT_DRAW },              // Mulcharmy Purulia
		{ 87126721, HT_DRAW },              // Mulcharmy Meowls
		{ 24508238, HT_GRAVE },             // D.D. Crow
		{ 52038441, HT_NEGATE_MONSTER },    // Ghost Mourner & Moonlit Chill
		{ 91800273, HT_FLOODGATE },         // Dimension Shifter
		{ 34267821, HT_FLOODGATE },         // Artifact Lancea
		{ 33854624, HT_GRAVE },             // Bystial Magnamhut
		{ 6637331, HT_GRAVE },              // Bystial Druiswurm
		{ 60242223, HT_GRAVE },             // Bystial Saronir
		{ 72656408, HT_GRAVE },             // Bystial Baldrake
		{ 67750322, HT_GRAVE },             // Skull Meister
		{ 38814750, HT_NEGATE_MONSTER },    // PSY-Framegear Gamma
		{ 40366667, HT_NEGATE_ACTIVATION }, // Dominus Impulse
		{ 97045737, HT_NEGATE_ACTIVATION }, // Dominus Purge
		{ 60643553, HT_GENERIC },           // Ghost Sister & Spooky Dogwood
		{ 62015408, HT_GENERIC },           // Ghost Reaper & Winter Cherries
		{ 46502744, HT_GENERIC },           // Retaliating "C"
		{ 87170768, HT_GENERIC },           // Contact "C"
		{ 58655504, HT_GENERIC },           // Gnomaterial
		{ 78661338, HT_GENERIC },           // Fantastical Dragon Phantazmay
	};
	return list;
}

inline uint32_t CanonicalCode(const CardDataC* card) {
	if(!card)
		return 0;
	if(card->alias && card->IsInArtworkOffsetRange())
		return card->alias;
	return card->getRealCode();
}

inline uint32_t CanonicalCode(uint32_t code) {
	const auto* data = gDataManager->GetCardData(code);
	return data ? CanonicalCode(data) : code;
}

inline HandtrapKind GetHandtrapKind(uint32_t canonical_code, bool* known = nullptr) {
	for(const auto& entry : KnownHandtraps()) {
		if(entry.code == canonical_code) {
			if(known)
				*known = true;
			return entry.kind;
		}
	}
	if(known)
		*known = false;
	return HT_GENERIC;
}

// Roles we can guess without asking, so a fresh deck is not a wall of "Not tagged".
inline Role GuessRole(uint32_t canonical_code) {
	bool known = false;
	GetHandtrapKind(canonical_code, &known);
	if(known)
		return ROLE_HANDTRAP;
	switch(canonical_code) {
	case 14532163: // Lightning Storm
	case 12580477: // Raigeki
	case 54693926: // Dark Ruler No More
	case 15693423: // Evenly Matched
	case 18144506: // Harpie's Feather Duster
	case 24299458: // Forbidden Droplet
	case 102380:   // Lava Golem
	case 55063751: // Gameciel, the Sea Turtle Kaiju
	case 48130397: // Super Polymerization
	case 8267140:  // Cosmic Cyclone
		return ROLE_BREAKER;
	case 24224830: // Called by the Grave
	case 65681983: // Crossout Designator
	case 25311006: // Triple Tactics Talent
	case 41420027: // Solemn Judgment
	case 23002292: // Red Reboot
		return ROLE_TECH;
	default:
		return ROLE_NONE;
	}
}

// Tags are remembered per deck, with the latest choice for a card also used as
// the default for that card in every other deck.
class RoleStore {
public:
	static RoleStore& Get() {
		static RoleStore store;
		return store;
	}
	Role Lookup(const std::string& deck, uint32_t code, bool* guessed = nullptr) {
		Load();
		if(guessed)
			*guessed = false;
		auto find = [&](const std::string& key, Role& out) {
			auto dit = roles.find(key);
			if(dit == roles.end())
				return false;
			auto cit = dit->second.find(code);
			if(cit == dit->second.end())
				return false;
			out = cit->second;
			return true;
		};
		Role role = ROLE_NONE;
		if(find(deck, role) || find(global_key, role))
			return role;
		role = GuessRole(code);
		if(guessed)
			*guessed = role != ROLE_NONE;
		return role;
	}
	void Assign(const std::string& deck, uint32_t code, Role role) {
		Load();
		roles[deck][code] = role;
		roles[global_key][code] = role;
		Save();
	}
	int GetSetting(const std::string& name, int def) {
		Load();
		auto it = settings.find(name);
		return it == settings.end() ? def : it->second;
	}
	void SetSetting(const std::string& name, int value) {
		Load();
		auto it = settings.find(name);
		if(it != settings.end() && it->second == value)
			return;
		settings[name] = value;
		Save();
	}
	std::vector<uint32_t> GetHandtrapPool() {
		Load();
		return handtrap_pool;
	}
	void SetHandtrapPool(std::vector<uint32_t> pool) {
		Load();
		handtrap_pool = std::move(pool);
		Save();
	}
private:
	static constexpr const char* global_key = "*";
	bool loaded{ false };
	std::map<std::string, std::map<uint32_t, Role>> roles;
	std::map<std::string, int> settings;
	std::vector<uint32_t> handtrap_pool;
	static epro::path_string FilePath() {
		return EPRO_TEXT("./config/deck_roles.json");
	}
	void Load() {
		if(loaded)
			return;
		loaded = true;
		handtrap_pool = { 14558127, 97268402, 10045474, 27204311, 94145021 };
		try {
			FileStream file{ FilePath(), FileStream::in };
			if(file.fail())
				return;
			const auto json = nlohmann::json::parse(file, nullptr, false);
			if(!json.is_object())
				return;
			if(auto it = json.find("decks"); it != json.end() && it->is_object()) {
				for(auto& deck : it->items()) {
					if(!deck.value().is_object())
						continue;
					auto& target = roles[deck.key()];
					for(auto& card : deck.value().items()) {
						if(!card.value().is_number_unsigned())
							continue;
						const auto role = card.value().get<uint32_t>();
						if(role >= ROLE_COUNT)
							continue;
						target[static_cast<uint32_t>(std::stoul(card.key()))] = static_cast<Role>(role);
					}
				}
			}
			if(auto it = json.find("settings"); it != json.end() && it->is_object()) {
				for(auto& setting : it->items()) {
					if(setting.value().is_number_integer())
						settings[setting.key()] = setting.value().get<int>();
				}
			}
			if(auto it = json.find("handtraps"); it != json.end() && it->is_array()) {
				handtrap_pool.clear();
				for(auto& code : *it) {
					if(code.is_number_unsigned())
						handtrap_pool.push_back(code.get<uint32_t>());
				}
			}
		} catch(...) {
			// a broken file only costs the tags, never the session
		}
	}
	void Save() {
		try {
			nlohmann::json json;
			auto& decks = json["decks"] = nlohmann::json::object();
			for(const auto& deck : roles) {
				auto& target = decks[deck.first] = nlohmann::json::object();
				for(const auto& card : deck.second)
					target[std::to_string(card.first)] = static_cast<uint32_t>(card.second);
			}
			json["settings"] = settings;
			json["handtraps"] = handtrap_pool;
			FileStream file{ FilePath(), FileStream::out | FileStream::trunc };
			if(file.fail())
				return;
			file << json.dump(1, '\t');
		} catch(...) {
		}
	}
};

// What the optimiser learns from saved hand tests: the hand a line started
// from and the board it ended on. Every saved replay of a deck adds one.
struct LearnedLine {
	std::vector<uint32_t> hand;
	std::vector<uint32_t> board;
	std::string replay;
};

class LineStore {
public:
	static LineStore& Get() {
		static LineStore store;
		return store;
	}
	void Add(const std::string& deck, std::vector<uint32_t> hand, std::vector<uint32_t> board, const std::string& replay) {
		Load();
		std::sort(hand.begin(), hand.end());
		std::sort(board.begin(), board.end());
		lines[deck].push_back({ std::move(hand), std::move(board), replay });
		Save();
	}
	const std::vector<LearnedLine>& Lines(const std::string& deck) {
		Load();
		return lines[deck];
	}
	// One line for the optimiser: how much has been learned and the board seen most often.
	std::wstring Describe(const std::string& deck) {
		const auto& learned = Lines(deck);
		if(learned.empty())
			return L"No routes learned yet. Save the replay of a hand test that went right and its end board is remembered for this deck.";
		std::map<std::vector<uint32_t>, int> boards;
		for(const auto& line : learned)
			++boards[line.board];
		const auto best = std::max_element(boards.begin(), boards.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
		std::wstring names;
		for(auto code : best->first) {
			if(!names.empty())
				names += L", ";
			const auto name = gDataManager->GetName(code);
			names.append(name.data(), name.size());
		}
		if(names.empty())
			names = L"an empty field";
		return epro::format(L"Learned {} route{} and {} different end board{}. Most common ({}x): {}.", learned.size(), learned.size() == 1 ? L"" : L"s",
							boards.size(), boards.size() == 1 ? L"" : L"s", best->second, names);
	}
private:
	bool loaded{ false };
	std::map<std::string, std::vector<LearnedLine>> lines;
	static epro::path_string FilePath() {
		return EPRO_TEXT("./config/learned_lines.json");
	}
	void Load() {
		if(loaded)
			return;
		loaded = true;
		try {
			FileStream file{ FilePath(), FileStream::in };
			if(file.fail())
				return;
			const auto json = nlohmann::json::parse(file, nullptr, false);
			if(!json.is_object())
				return;
			for(auto& deck : json.items()) {
				if(!deck.value().is_array())
					continue;
				for(auto& entry : deck.value()) {
					LearnedLine line;
					line.hand = entry.value("hand", std::vector<uint32_t>{});
					line.board = entry.value("board", std::vector<uint32_t>{});
					line.replay = entry.value("replay", std::string{});
					lines[deck.key()].push_back(std::move(line));
				}
			}
		} catch(...) {
		}
	}
	void Save() {
		try {
			nlohmann::json json = nlohmann::json::object();
			for(const auto& deck : lines) {
				auto& target = json[deck.first] = nlohmann::json::array();
				for(const auto& line : deck.second)
					target.push_back({ { "hand", line.hand }, { "board", line.board }, { "replay", line.replay } });
			}
			FileStream file{ FilePath(), FileStream::out | FileStream::trunc };
			if(!file.fail())
				file << json.dump(1, '\t');
		} catch(...) {
		}
	}
};

struct DeckEntry {
	uint32_t code{};
	const CardDataC* card{};
	int count{};
	Role role{ ROLE_NONE };
	bool guessed{ false };
};

struct HandOdds {
	int hand{};
	double starter{};       // at least one starter
	double starter_ext{};   // a starter plus an extender
	double handtrap{};      // at least one hand trap
	double handtrap_avg{};  // average hand traps held
	double breaker{};       // at least one board breaker
	double dead{};          // at least one brick / engine requirement
	double dead2{};         // two or more of them
	double playable{};      // a starter and at most one dead card
};

struct DeckAnalysis {
	std::vector<DeckEntry> entries;
	int total{};
	int counts[ROLE_COUNT]{};
	HandOdds first;  // going first, 5 cards
	HandOdds second; // going second, 6 cards
	std::vector<std::wstring> advice;
};

inline double Choose(int n, int k) {
	if(k < 0 || n < 0 || k > n)
		return 0.0;
	k = std::min(k, n - k);
	double result = 1.0;
	for(int i = 1; i <= k; ++i)
		result = result * (n - k + i) / i;
	return result;
}

// Chance of at least `need` hits among `hits` copies in a `total` card deck.
inline double AtLeast(int total, int hits, int hand, int need = 1) {
	if(total <= 0 || hand <= 0)
		return 0.0;
	hand = std::min(hand, total);
	const double all = Choose(total, hand);
	double below = 0.0;
	for(int i = 0; i < need; ++i)
		below += Choose(hits, i) * Choose(total - hits, hand - i);
	return std::clamp(1.0 - below / all, 0.0, 1.0);
}

inline HandOdds ComputeOdds(int total, int starters, int extenders, int handtraps, int breakers, int dead, int hand) {
	HandOdds odds;
	odds.hand = hand;
	if(total <= 0)
		return odds;
	hand = std::min(hand, total);
	const double all = Choose(total, hand);
	odds.starter = AtLeast(total, starters, hand);
	odds.handtrap = AtLeast(total, handtraps, hand);
	odds.handtrap_avg = static_cast<double>(hand) * handtraps / total;
	odds.breaker = AtLeast(total, breakers, hand);
	odds.dead = AtLeast(total, dead, hand);
	odds.dead2 = AtLeast(total, dead, hand, 2);
	// a starter and at most one dead card
	{
		const int rest = total - starters - dead;
		double sum = 0.0;
		for(int s = 1; s <= std::min(starters, hand); ++s) {
			for(int d = 0; d <= std::min({ dead, 1, hand - s }); ++d)
				sum += Choose(starters, s) * Choose(dead, d) * Choose(rest, hand - s - d);
		}
		odds.playable = std::clamp(sum / all, 0.0, 1.0);
	}
	// a starter together with an extender
	{
		const int rest = total - starters - extenders;
		double sum = 0.0;
		for(int s = 1; s <= std::min(starters, hand); ++s) {
			for(int e = 1; e <= std::min(extenders, hand - s); ++e)
				sum += Choose(starters, s) * Choose(extenders, e) * Choose(rest, hand - s - e);
		}
		odds.starter_ext = std::clamp(sum / all, 0.0, 1.0);
	}
	return odds;
}

inline int Percent(double value) {
	return static_cast<int>(value * 100.0 + 0.5);
}

inline const wchar_t* Grade(double playable) {
	if(playable >= 0.90) return L"Excellent";
	if(playable >= 0.82) return L"Good";
	if(playable >= 0.72) return L"Okay";
	return L"Inconsistent";
}

inline std::wstring EntryName(const DeckEntry& entry) {
	const auto name = gDataManager->GetName(entry.code);
	return std::wstring{ name.data(), name.size() };
}

inline DeckAnalysis AnalyzeDeck(const Deck& deck, const std::string& deck_name) {
	DeckAnalysis result;
	auto& store = RoleStore::Get();
	for(const auto* card : deck.main) {
		const auto code = CanonicalCode(card);
		auto it = std::find_if(result.entries.begin(), result.entries.end(), [code](const DeckEntry& e) { return e.code == code; });
		if(it == result.entries.end()) {
			DeckEntry entry;
			entry.code = code;
			entry.card = card;
			entry.count = 1;
			entry.role = store.Lookup(deck_name, code, &entry.guessed);
			result.entries.push_back(entry);
		} else
			++it->count;
	}
	// untagged cards first: they are the ones the player still has to look at
	std::stable_sort(result.entries.begin(), result.entries.end(), [](const DeckEntry& a, const DeckEntry& b) {
		const auto rank = [](const DeckEntry& e) { return e.role == ROLE_NONE ? -1 : static_cast<int>(e.role); };
		return rank(a) < rank(b);
	});
	for(const auto& entry : result.entries) {
		result.total += entry.count;
		result.counts[entry.role] += entry.count;
	}
	const int total = result.total;
	const int starters = result.counts[ROLE_STARTER];
	const int extenders = result.counts[ROLE_EXTENDER];
	const int handtraps = result.counts[ROLE_HANDTRAP];
	const int breakers = result.counts[ROLE_BREAKER];
	const int dead = result.counts[ROLE_BRICK] + result.counts[ROLE_ENGINE_REQ];
	const int untagged = result.counts[ROLE_NONE];
	result.first = ComputeOdds(total, starters, extenders, handtraps, breakers, dead, 5);
	result.second = ComputeOdds(total, starters, extenders, handtraps, breakers, dead, 6);

	auto& advice = result.advice;
	if(total == 0) {
		advice.emplace_back(L"Add cards to the main deck to get started.");
		return result;
	}
	if(starters == 0) {
		advice.emplace_back(L"Tag your starters first: tap a card in the list, then tap \"Starter\". Everything else is worked out from that.");
		return result;
	}
	if(untagged > 0)
		advice.push_back(epro::format(L"{} card{} still not tagged. Tag them so the numbers below are accurate.", untagged, untagged == 1 ? L" is" : L"s are"));

	// the first card we would cut, and the first we would add
	auto pick = [&](std::initializer_list<Role> order, bool to_add) -> const DeckEntry* {
		for(auto role : order) {
			const DeckEntry* best = nullptr;
			for(const auto& entry : result.entries) {
				if(entry.role != role)
					continue;
				if(to_add) {
					if(entry.count >= 3)
						continue;
					if(!best || entry.count > best->count)
						best = &entry;
				} else if(!best || entry.count > best->count)
					best = &entry;
			}
			if(best)
				return best;
		}
		return nullptr;
	};
	const auto* cut = pick({ ROLE_BRICK, ROLE_TECH, ROLE_NONE, ROLE_ENGINE_REQ, ROLE_BREAKER }, false);
	const auto* add = pick({ ROLE_STARTER }, true);
	const bool cut_is_dead = cut && (cut->role == ROLE_BRICK || cut->role == ROLE_ENGINE_REQ);

	if(total > 40) {
		const auto trimmed = ComputeOdds(40, starters, extenders, handtraps, breakers, std::min(dead, 40 - starters), 5);
		if(trimmed.starter - result.first.starter >= 0.005) {
			auto line = epro::format(L"You run {} cards. Cutting {} to get to 40 lifts your chance to open a starter from {}% to {}%.",
									 total, total - 40, Percent(result.first.starter), Percent(trimmed.starter));
			if(cut)
				line += epro::format(L" Start with {}.", EntryName(*cut));
			advice.push_back(std::move(line));
		}
	}
	if(result.first.starter < 0.85) {
		int need = starters;
		while(need < total && AtLeast(total, need, 5) < 0.85)
			++need;
		auto line = epro::format(L"You open a starter in only {}% of hands. {} more starter{} would get you past 85%.",
								 Percent(result.first.starter), need - starters, need - starters == 1 ? L"" : L"s");
		if(add)
			line += epro::format(L" {} is at {} cop{}.", EntryName(*add), add->count, add->count == 1 ? L"y" : L"ies");
		advice.push_back(std::move(line));
	} else if(result.first.starter >= 0.97 && starters > 14) {
		advice.push_back(epro::format(L"{} starters already gives {}%. You can turn one or two into hand traps without losing consistency.",
									  starters, Percent(result.first.starter)));
	}
	if(dead > 0 && result.first.dead >= 0.30) {
		const auto fewer = ComputeOdds(total, starters, extenders, handtraps, breakers, dead - 1, 5);
		advice.push_back(epro::format(L"{}% of your hands hold a card you never want to draw ({} in the deck). One fewer brings that to {}%.",
									  Percent(result.first.dead), dead, Percent(fewer.dead)));
	}
	if(handtraps < 6) {
		const auto more = AtLeast(total, 9, 5);
		advice.push_back(epro::format(L"Only {} hand trap{}: you see one in {}% of hands. Most decks run 9 to 12, which is about {}%.",
									  handtraps, handtraps == 1 ? L"" : L"s", Percent(result.first.handtrap), Percent(more)));
	} else if(handtraps > 15 && result.first.starter < 0.90) {
		advice.push_back(epro::format(L"{} hand traps is a lot while starters are at {}%. Trade a few for starters.",
									  handtraps, Percent(result.first.starter)));
	}
	if(breakers == 0)
		advice.emplace_back(L"No board breakers. If you lose the coin toss you have nothing to clear an established board.");
	// the single best one-for-one swap
	if(cut && result.first.playable < 0.95) {
		const int new_dead = cut_is_dead ? dead - 1 : dead;
		const auto swapped = ComputeOdds(total, starters + 1, extenders, handtraps, breakers, new_dead, 5);
		if(swapped.playable - result.first.playable >= 0.01) {
			advice.push_back(epro::format(L"Best single swap: take out 1 {} and add 1 {}. Playable hands go from {}% to {}%.",
										  EntryName(*cut), add ? EntryName(*add) : std::wstring{ L"more starter" },
										  Percent(result.first.playable), Percent(swapped.playable)));
		}
	}
	if(advice.empty())
		advice.emplace_back(L"Nothing to fix. The ratios are solid.");
	return result;
}

inline std::wstring FormatOdds(const DeckAnalysis& analysis, bool going_second) {
	const auto& odds = going_second ? analysis.second : analysis.first;
	const int dead = analysis.counts[ROLE_BRICK] + analysis.counts[ROLE_ENGINE_REQ];
	std::wstring text;
	text += epro::format(L"Playable hand: {}%  ({})\n", Percent(odds.playable), Grade(odds.playable));
	text += epro::format(L"Open a starter: {}%\n", Percent(odds.starter));
	text += epro::format(L"Starter + extender: {}%\n", Percent(odds.starter_ext));
	text += epro::format(L"Open a hand trap: {}%  (avg {:.1f})\n", Percent(odds.handtrap), odds.handtrap_avg);
	if(going_second)
		text += epro::format(L"Open a board breaker: {}%\n", Percent(odds.breaker));
	text += epro::format(L"Draw a dead card: {}%", Percent(odds.dead));
	if(dead > 1)
		text += epro::format(L"  (two or more: {}%)", Percent(odds.dead2));
	return text;
}

inline std::wstring FormatCounts(const DeckAnalysis& analysis) {
	return epro::format(L"{} cards:  {} starter  |  {} extender  |  {} hand trap  |  {} breaker  |  {} never-draw  |  {} other  |  {} not tagged",
						analysis.total, analysis.counts[ROLE_STARTER], analysis.counts[ROLE_EXTENDER], analysis.counts[ROLE_HANDTRAP],
						analysis.counts[ROLE_BREAKER], analysis.counts[ROLE_BRICK] + analysis.counts[ROLE_ENGINE_REQ],
						analysis.counts[ROLE_TECH], analysis.counts[ROLE_NONE]);
}

inline std::wstring FormatAdvice(const DeckAnalysis& analysis) {
	std::wstring text;
	for(const auto& line : analysis.advice) {
		if(!text.empty())
			text += L"\n";
		text += L"- ";
		text += line;
	}
	return text;
}

}
}

#endif //DECK_OPTIMIZER_H
