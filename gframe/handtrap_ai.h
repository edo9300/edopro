#ifndef HANDTRAP_AI_H
#define HANDTRAP_AI_H
// Built-in hand test opponent. It never plays a turn of its own: it holds the
// hand traps it was dealt and answers every prompt the core sends to player 1.
//   Easy  - uses a hand trap the first moment the game allows it.
//   Smart - holds each one for the play it is best against, judged from the
//           roles the player gave their cards in the deck optimiser.
// Header only so explicit-source-list builds (Android, iOS) need no changes.

#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <vector>
#include "common.h"
#include "core_utils.h"
#include "deck_optimizer.h"

namespace ygo {
namespace rka {

class HandtrapAI {
public:
	enum Mode : uint8_t {
		OFF = 0,
		EASY,
		SMART,
	};
	// 3: starter, 2: needed for the combo, 1: nice to stop, 0: not worth a hand trap
	using ImportanceFn = std::function<int(uint32_t)>;

	static constexpr uint8_t ai_player = 1;

	void Reset(Mode new_mode, ImportanceFn importance_fn) {
		mode = new_mode;
		importance = std::move(importance_fn);
		chain.clear();
		skipped.clear();
		acted_this_chain = false;
		has_wanted = false;
	}
	Mode GetMode() const { return mode; }
	bool Enabled() const { return mode != OFF; }

	// Called for every message the core produces, so the AI knows what is on the chain.
	void Observe(const CoreUtils::Packet& packet) {
		if(mode == OFF)
			return;
		const auto* pbuf = packet.data();
		switch(packet.message) {
		case MSG_CHAINING: {
			if(packet.buff_size() < 20)
				break;
			Link link;
			link.code = CanonicalCode(BufferIO::Read<uint32_t>(pbuf));
			CoreUtils::ReadLocInfo(pbuf, false);
			link.controler = BufferIO::Read<uint8_t>(pbuf);
			link.location = BufferIO::Read<uint8_t>(pbuf);
			link.sequence = BufferIO::Read<uint32_t>(pbuf);
			chain.push_back(link);
			break;
		}
		case MSG_CHAIN_SOLVED: {
			if(!chain.empty())
				chain.pop_back();
			break;
		}
		case MSG_CHAIN_END: {
			chain.clear();
			acted_this_chain = false;
			has_wanted = false;
			break;
		}
		case MSG_NEW_TURN: {
			chain.clear();
			skipped.clear();
			acted_this_chain = false;
			has_wanted = false;
			break;
		}
		default:
			break;
		}
	}

	static bool IsSelection(uint8_t message) {
		switch(message) {
		case MSG_SELECT_BATTLECMD:
		case MSG_SELECT_IDLECMD:
		case MSG_SELECT_EFFECTYN:
		case MSG_SELECT_YESNO:
		case MSG_SELECT_OPTION:
		case MSG_SELECT_CARD:
		case MSG_SELECT_TRIBUTE:
		case MSG_SELECT_UNSELECT_CARD:
		case MSG_SELECT_CHAIN:
		case MSG_SELECT_PLACE:
		case MSG_SELECT_DISFIELD:
		case MSG_SELECT_POSITION:
		case MSG_SELECT_COUNTER:
		case MSG_SELECT_SUM:
		case MSG_SORT_CARD:
		case MSG_SORT_CHAIN:
		case MSG_ROCK_PAPER_SCISSORS:
		case MSG_ANNOUNCE_RACE:
		case MSG_ANNOUNCE_ATTRIB:
		case MSG_ANNOUNCE_CARD:
		case MSG_ANNOUNCE_NUMBER:
			return true;
		default:
			return false;
		}
	}

	// True when the prompt is addressed to the AI's side of the field.
	static bool IsForAI(const CoreUtils::Packet& packet) {
		return IsSelection(packet.message) && packet.buff_size() > 0 && packet.data()[0] == ai_player;
	}

	// Fills `response` with the AI's answer. Returns false when it has none,
	// in which case the prompt is handed to the player instead.
	bool Respond(const CoreUtils::Packet& packet, std::vector<uint8_t>& response) {
		response.clear();
		if(mode == OFF || !IsForAI(packet))
			return false;
		const auto* pbuf = packet.data();
		const auto* end = pbuf + packet.buff_size();
		++pbuf; // player
		switch(packet.message) {
		case MSG_SELECT_CHAIN:
			return RespondChain(pbuf, end, response);
		case MSG_SELECT_CARD:
		case MSG_SELECT_TRIBUTE:
			return RespondCard(pbuf, end, response, packet.message == MSG_SELECT_TRIBUTE);
		case MSG_SELECT_UNSELECT_CARD:
			return RespondUnselect(pbuf, end, response);
		case MSG_SELECT_IDLECMD: {
			// nothing to do on its own turn: pass
			if(!Skip(pbuf, end, 10) || !Skip(pbuf, end, 10) || !Skip(pbuf, end, 7) ||
			   !Skip(pbuf, end, 10) || !Skip(pbuf, end, 10) || !Skip(pbuf, end, 19) || end - pbuf < 2)
				return false;
			const auto to_bp = pbuf[0];
			const auto to_ep = pbuf[1];
			Put<int32_t>(response, to_ep ? 7 : (to_bp ? 6 : 7));
			return true;
		}
		case MSG_SELECT_BATTLECMD: {
			if(!Skip(pbuf, end, 19) || !Skip(pbuf, end, 8) || end - pbuf < 2)
				return false;
			const auto to_m2 = pbuf[0];
			const auto to_ep = pbuf[1];
			Put<int32_t>(response, to_ep ? 3 : (to_m2 ? 2 : 3));
			return true;
		}
		case MSG_SELECT_EFFECTYN:
		case MSG_SELECT_YESNO:
			Put<int32_t>(response, 1);
			return true;
		case MSG_SELECT_OPTION:
		case MSG_ANNOUNCE_NUMBER:
			Put<int32_t>(response, 0);
			return true;
		case MSG_SELECT_PLACE:
		case MSG_SELECT_DISFIELD:
			return RespondPlace(pbuf, end, response);
		case MSG_SELECT_POSITION: {
			if(end - pbuf < 5)
				return false;
			const uint8_t positions = pbuf[4];
			int32_t chosen = 0;
			if(positions & POS_FACEUP_ATTACK)
				chosen = POS_FACEUP_ATTACK;
			else if(positions & POS_FACEUP_DEFENSE)
				chosen = POS_FACEUP_DEFENSE;
			else {
				for(int bit = 1; bit < 0x10; bit <<= 1) {
					if(positions & bit) {
						chosen = bit;
						break;
					}
				}
			}
			if(!chosen)
				return false;
			Put<int32_t>(response, chosen);
			return true;
		}
		case MSG_SELECT_COUNTER: {
			if(end - pbuf < 8)
				return false;
			pbuf += 2;
			int remaining = BufferIO::Read<uint16_t>(pbuf);
			const auto count = BufferIO::Read<uint32_t>(pbuf);
			if(static_cast<size_t>(end - pbuf) < static_cast<size_t>(count) * 9)
				return false;
			for(uint32_t i = 0; i < count; ++i) {
				pbuf += 7;
				const int available = BufferIO::Read<uint16_t>(pbuf);
				const int take = remaining < available ? remaining : available;
				remaining -= take;
				Put<int16_t>(response, static_cast<int16_t>(take));
			}
			return remaining == 0;
		}
		case MSG_SORT_CARD:
		case MSG_SORT_CHAIN:
			Put<int8_t>(response, -1);
			return true;
		case MSG_ROCK_PAPER_SCISSORS:
			Put<int32_t>(response, 1);
			return true;
		case MSG_ANNOUNCE_RACE: {
			if(end - pbuf < 9)
				return false;
			int count = BufferIO::Read<uint8_t>(pbuf);
			const auto available = BufferIO::Read<uint64_t>(pbuf);
			uint64_t chosen = 0;
			for(int bit = 0; bit < 64 && count > 0; ++bit) {
				if(available & (uint64_t{ 1 } << bit)) {
					chosen |= uint64_t{ 1 } << bit;
					--count;
				}
			}
			Put<uint64_t>(response, chosen);
			return count == 0;
		}
		case MSG_ANNOUNCE_ATTRIB: {
			if(end - pbuf < 5)
				return false;
			int count = BufferIO::Read<uint8_t>(pbuf);
			const auto available = BufferIO::Read<uint32_t>(pbuf);
			uint32_t chosen = 0;
			for(int bit = 0; bit < 32 && count > 0; ++bit) {
				if(available & (uint32_t{ 1 } << bit)) {
					chosen |= uint32_t{ 1 } << bit;
					--count;
				}
			}
			Put<uint32_t>(response, chosen);
			return count == 0;
		}
		case MSG_ANNOUNCE_CARD: {
			const auto* link = PlayerLink();
			Put<int32_t>(response, static_cast<int32_t>(link ? link->code : 14558127));
			return true;
		}
		default:
			// MSG_SELECT_SUM and anything new: let the player decide
			return false;
		}
	}

private:
	struct Link {
		uint32_t code{};
		uint8_t controler{};
		uint8_t location{};
		uint32_t sequence{};
	};
	struct Option {
		uint32_t code{};
		CoreUtils::loc_info info{};
	};

	Mode mode{ OFF };
	ImportanceFn importance;
	std::vector<Link> chain;
	std::map<uint32_t, int> skipped; // hand trap -> chances passed up this turn
	bool acted_this_chain{ false };
	bool has_wanted{ false };
	Link wanted; // what the hand trap that was just used should be aimed at

	static constexpr int patience = 3;

	template<typename T>
	static void Put(std::vector<uint8_t>& out, T value) {
		const auto size = out.size();
		out.resize(size + sizeof(T));
		std::memcpy(out.data() + size, &value, sizeof(T));
	}
	// skips a "count, then count fixed size entries" list
	static bool Skip(const uint8_t*& pbuf, const uint8_t* end, size_t entry_size) {
		if(end - pbuf < 4)
			return false;
		const auto count = BufferIO::Read<uint32_t>(pbuf);
		if(static_cast<size_t>(end - pbuf) < static_cast<size_t>(count) * entry_size)
			return false;
		pbuf += static_cast<size_t>(count) * entry_size;
		return true;
	}
	static bool ReadOptions(const uint8_t*& pbuf, const uint8_t* end, std::vector<Option>& options, size_t trailing) {
		if(end - pbuf < 4)
			return false;
		const auto count = BufferIO::Read<uint32_t>(pbuf);
		if(static_cast<size_t>(end - pbuf) < static_cast<size_t>(count) * (14 + trailing))
			return false;
		options.resize(count);
		for(auto& option : options) {
			option.code = CanonicalCode(BufferIO::Read<uint32_t>(pbuf));
			option.info = CoreUtils::ReadLocInfo(pbuf, false);
			pbuf += trailing;
		}
		return true;
	}
	// the player's link the AI would be responding to, if any
	const Link* PlayerLink() const {
		if(chain.empty() || chain.back().controler == ai_player)
			return nullptr;
		return &chain.back();
	}
	int Importance(uint32_t code) const {
		return importance ? importance(code) : 2;
	}

	bool WantsToUse(uint32_t code) {
		const auto* link = PlayerLink();
		const auto kind = GetHandtrapKind(code);
		const int worth = link ? Importance(link->code) : 0;
		auto& passes = skipped[code];
		bool use = false;
		switch(kind) {
		case HT_BOARD_WIPE:
		case HT_DRAW:
		case HT_FLOODGATE:
			// these are best as early as the game allows
			use = true;
			break;
		case HT_NEGATE_MONSTER:
			if(link && link->location == LOCATION_MZONE)
				use = worth >= 2 || passes >= patience;
			else if(!link)
				use = passes >= patience * 3;
			break;
		case HT_GRAVE:
			if(link && link->location == LOCATION_GRAVE)
				use = worth >= 1 || passes >= patience;
			else if(!link)
				use = passes >= patience * 3;
			break;
		case HT_NEGATE_ACTIVATION:
		case HT_GENERIC:
		default:
			if(link)
				use = worth >= 2 || passes >= patience;
			break;
		}
		if(use && acted_this_chain && link) {
			// one hand trap per play is enough
			use = false;
		}
		if(!use)
			++passes;
		return use;
	}

	bool RespondChain(const uint8_t* pbuf, const uint8_t* end, std::vector<uint8_t>& response) {
		if(end - pbuf < 10)
			return false;
		++pbuf; // special count
		const auto forced = BufferIO::Read<uint8_t>(pbuf);
		pbuf += 8; // hint timings
		std::vector<Option> options;
		if(!ReadOptions(pbuf, end, options, 9))
			return false;
		int chosen = -1;
		if(mode == EASY) {
			if(!options.empty())
				chosen = 0;
		} else {
			std::vector<uint32_t> considered;
			for(size_t i = 0; i < options.size() && chosen < 0; ++i) {
				const auto code = options[i].code;
				// three copies of the same card are one decision, not three
				if(std::find(considered.begin(), considered.end(), code) != considered.end())
					continue;
				considered.push_back(code);
				if(WantsToUse(code))
					chosen = static_cast<int>(i);
			}
		}
		if(chosen < 0 && forced && !options.empty())
			chosen = 0;
		if(chosen >= 0) {
			acted_this_chain = true;
			const auto* link = PlayerLink();
			has_wanted = link != nullptr;
			if(link)
				wanted = *link;
		}
		Put<int32_t>(response, chosen);
		return true;
	}

	// best cards first: what the hand trap was used against, then the player's most important cards
	std::vector<uint32_t> RankOptions(const std::vector<Option>& options) const {
		std::vector<uint32_t> order(options.size());
		std::vector<int> score(options.size());
		for(uint32_t i = 0; i < options.size(); ++i) {
			order[i] = i;
			const auto& option = options[i];
			int value = 0;
			if(mode == SMART && option.info.controler != ai_player) {
				value = 1 + Importance(option.code);
				if(has_wanted && option.info.controler == wanted.controler && option.info.location == wanted.location &&
				   option.info.sequence == wanted.sequence)
					value += 100;
				else if(has_wanted && option.code == wanted.code)
					value += 50;
			}
			score[i] = value;
		}
		std::stable_sort(order.begin(), order.end(), [&score](uint32_t a, uint32_t b) { return score[a] > score[b]; });
		return order;
	}

	bool RespondCard(const uint8_t* pbuf, const uint8_t* end, std::vector<uint8_t>& response, bool tribute) {
		if(end - pbuf < 9)
			return false;
		++pbuf; // cancelable
		const auto min = BufferIO::Read<uint32_t>(pbuf);
		const auto max = BufferIO::Read<uint32_t>(pbuf);
		std::vector<Option> options;
		if(tribute) {
			if(end - pbuf < 4)
				return false;
			const auto count = BufferIO::Read<uint32_t>(pbuf);
			if(static_cast<size_t>(end - pbuf) < static_cast<size_t>(count) * 11)
				return false;
			options.resize(count);
			for(auto& option : options) {
				option.code = CanonicalCode(BufferIO::Read<uint32_t>(pbuf));
				option.info.controler = BufferIO::Read<uint8_t>(pbuf);
				option.info.location = BufferIO::Read<uint8_t>(pbuf);
				option.info.sequence = BufferIO::Read<uint32_t>(pbuf);
				++pbuf; // release param
			}
		} else if(!ReadOptions(pbuf, end, options, 0))
			return false;
		uint32_t amount = min ? min : 1;
		if(amount > max)
			amount = max;
		if(amount > options.size())
			return false;
		const auto order = RankOptions(options);
		Put<int32_t>(response, 0);
		Put<uint32_t>(response, amount);
		for(uint32_t i = 0; i < amount; ++i)
			Put<uint32_t>(response, order[i]);
		return true;
	}

	bool RespondUnselect(const uint8_t* pbuf, const uint8_t* end, std::vector<uint8_t>& response) {
		if(end - pbuf < 10)
			return false;
		const auto finishable = BufferIO::Read<uint8_t>(pbuf);
		const auto cancelable = BufferIO::Read<uint8_t>(pbuf);
		pbuf += 8; // min, max
		std::vector<Option> options;
		if(!ReadOptions(pbuf, end, options, 0))
			return false;
		if(finishable || options.empty()) {
			if(!finishable && !cancelable)
				return false;
			Put<int32_t>(response, -1);
			return true;
		}
		const auto order = RankOptions(options);
		Put<int32_t>(response, 1);
		Put<int32_t>(response, static_cast<int32_t>(order[0]));
		return true;
	}

	bool RespondPlace(const uint8_t* pbuf, const uint8_t* end, std::vector<uint8_t>& response) {
		if(end - pbuf < 5)
			return false;
		int count = BufferIO::Read<uint8_t>(pbuf);
		if(count == 0)
			count = 1;
		const auto flag = BufferIO::Read<uint32_t>(pbuf); // set bit = zone not available
		static constexpr uint8_t mzone_order[] = { 2, 1, 3, 0, 4, 5, 6 };
		static constexpr uint8_t szone_order[] = { 2, 1, 3, 0, 4, 5, 6, 7 };
		for(int side = 0; side < 2 && count > 0; ++side) {
			const uint32_t zones = (flag >> (side * 16)) & 0xffff;
			const uint8_t player = side == 0 ? ai_player : 1 - ai_player;
			for(auto seq : mzone_order) {
				if(count > 0 && !(zones & (1u << seq))) {
					response.push_back(player);
					response.push_back(LOCATION_MZONE);
					response.push_back(seq);
					--count;
				}
			}
			for(auto seq : szone_order) {
				if(count > 0 && !(zones & (0x100u << seq))) {
					response.push_back(player);
					response.push_back(LOCATION_SZONE);
					response.push_back(seq);
					--count;
				}
			}
		}
		return count == 0;
	}
};

}
}

#endif //HANDTRAP_AI_H
