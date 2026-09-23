#include "game.h"
#include "data_manager.h"
#include "common.h"
#include "client_card.h"
#include "fmt.h"

namespace ygo {

void ClientCard::UpdateDrawCoordinates(bool setTrans) {
	mainGame->dField.GetCardDrawCoordinates(this, &curPos, &curRot, setTrans);
}
template <typename T>
static inline bool IsDifferent(T& what, T _new) {
	return std::exchange(what, _new) != what;
}
void ClientCard::SetCode(uint32_t new_code) {
	if(!IsDifferent(code, new_code))
		return;
	if(location != LOCATION_HAND)
		return;
	if(mainGame->dInfo.isCatchingUp)
		return;
	mainGame->dField.MoveCard(this, 5);
}
#define CHECK_AND_SET(_query, value) do { if(query.flag & _query) value = query.value; } while(0)
void ClientCard::UpdateInfo(const CoreUtils::Query& query) {
	CHECK_AND_SET(QUERY_ALIAS, alias);
	CHECK_AND_SET(QUERY_TYPE, type);
	CHECK_AND_SET(QUERY_ATTRIBUTE, attribute);
	CHECK_AND_SET(QUERY_RACE, race);
	CHECK_AND_SET(QUERY_BASE_ATTACK, base_attack);
	CHECK_AND_SET(QUERY_BASE_DEFENSE, base_defense);
	CHECK_AND_SET(QUERY_REASON, reason);
	CHECK_AND_SET(QUERY_OWNER, owner);
	CHECK_AND_SET(QUERY_STATUS, status);
	CHECK_AND_SET(QUERY_COVER, cover);
	if(query.flag & QUERY_CODE)
		SetCode(query.code);
	if(query.flag & QUERY_POSITION) {
		if(IsDifferent(position, static_cast<uint8_t>(query.position)) && (location & (LOCATION_EXTRA | LOCATION_REMOVED)))
			mainGame->dField.MoveCard(this, 1);
	}
	if(query.flag & QUERY_LEVEL) {
		if(IsDifferent(level, query.level) || lvstring.empty())
			lvstring = epro::format(L"L{}", level);
	}
	if(query.flag & QUERY_RANK) {
		if(IsDifferent(rank, query.rank) || rkstring.empty())
			rkstring = epro::format(L"R{}", rank);
	}
	if(query.flag & QUERY_ATTACK) {
		if(IsDifferent(attack, query.attack) || atkstring.empty()) {
			if(attack < 0) {
				atkstring = L"?";
			} else
				atkstring = epro::to_wstring(attack);
		}
	}
	if(query.flag & QUERY_DEFENSE) {
		if(IsDifferent(defense, query.defense) || defstring.empty()) {
			if(defense < 0) {
				defstring = L"?";
			} else
				defstring = epro::to_wstring(defense);
		}
	}
	/*if(query.flag & QUERY_REASON_CARD) {

	}*/
	if(query.flag & QUERY_EQUIP_CARD) {
		const auto& equip = query.equip_card;
		ClientCard* ecard = mainGame->dField.GetCard(mainGame->LocalPlayer(equip.controler), equip.location, equip.sequence);
		if(ecard) {
			equipTarget = ecard;
			ecard->equipped.insert(this);
		}
	}
	if(query.flag & QUERY_TARGET_CARD) {
		for(const auto& card : query.target_cards) {
			ClientCard* tcard = mainGame->dField.GetCard(mainGame->LocalPlayer(card.controler), card.location, card.sequence);
			cardTarget.insert(tcard);
			tcard->ownerTarget.insert(this);
		}
	}
	if(query.flag & QUERY_OVERLAY_CARD) {
		size_t i = 0;
		for(auto& overlay_code : query.overlay_cards) {
			overlayed[i++]->SetCode(overlay_code);
		}
	}
	if(query.flag & QUERY_COUNTERS) {
		for(auto& counter : query.counters) {
			int ctype = counter & 0xffff;
			int ccount = counter >> 16;
			counters[ctype] = ccount;
		}
	}
	if(query.flag & QUERY_IS_PUBLIC) {
		if(IsDifferent(is_public, !!query.is_public) && !mainGame->dInfo.isCatchingUp)
			mainGame->dField.MoveCard(this, 5);
	}
	if(query.flag & QUERY_LSCALE) {
		if(IsDifferent(lscale, query.lscale) || lscstring.empty())
			lscstring = epro::to_wstring(lscale);
	}
	if(query.flag & QUERY_RSCALE) {
		if(IsDifferent(rscale, query.rscale) || rscstring.empty())
			rscstring = epro::to_wstring(rscale);
	}
	if(query.flag & QUERY_LINK) {
		if(IsDifferent(link, query.link) || linkstring.empty())
			linkstring = epro::format(L"L{}", link);
		link_marker = query.link_marker;
	}
}
void ClientCard::ClearTarget() {
	for(auto& pcard : cardTarget) {
		pcard->is_showtarget = false;
		pcard->ownerTarget.erase(this);
	}
	for(auto& pcard : ownerTarget) {
		pcard->is_showtarget = false;
		pcard->cardTarget.erase(this);
	}
	cardTarget.clear();
	ownerTarget.clear();
}
bool ClientCard::client_card_sort(ClientCard* c1, ClientCard* c2) {
	// attached cards are sorted alongside the thing they are attached to
	ClientCard* e1 = (c1->overlayTarget != nullptr) ? c1->overlayTarget : c1;
	ClientCard* e2 = (c2->overlayTarget != nullptr) ? c2->overlayTarget : c2;
	// if they are attached to the same thing, shortcut:
	if (e1 == e2) {
		if ((c1->overlayTarget != nullptr) != (c2->overlayTarget != nullptr)) {
			// if only one is attached, the non-attached card comes first
			return c1->overlayTarget == nullptr;
		}
		// if both are attached, order by sequence
		return c1->sequence < c2->sequence;
	}
	// player cards go before opponent cards
	if (e1->controler != e2->controler)
		return e1->controler < e2->controler;
	// cards are grouped by location
	if (e1->location != e2->location)
		return e1->location < e2->location;

	// sorting behavior differs for each location
	if (e1->location & (LOCATION_DECK | LOCATION_EXTRA)) {
		// face-up cards go before face-down cards
		auto fu1 = e1->is_reversed;
		auto fu2 = e2->is_reversed;
		if (fu1 != fu2)
			return fu1;
		else if (fu1) {
			// face-up cards stay in (reverse) order
			return e1->sequence > e2->sequence;
		} else {
			CardDataC const* data1 = gDataManager->GetCardData(e1->code);
			CardDataC const* data2 = gDataManager->GetCardData(e2->code);
			if (data1 && data2) {
				auto basetype = [](uint32_t t) {
					return std::make_pair(
						t & (TYPE_MONSTER | TYPE_SPELL | TYPE_TRAP),
						t & (TYPE_NORMAL | TYPE_EFFECT | TYPE_RITUAL | TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ | TYPE_LINK |
							 TYPE_QUICKPLAY | TYPE_CONTINUOUS | TYPE_EQUIP | TYPE_FIELD | TYPE_COUNTER)); };
				auto [base1, extra1] = basetype(data1->type);
				auto [base2, extra2] = basetype(data2->type);
				// first, group by monster/spell/trap
				if (base1 != base2)
					return base1 < base2;
				// then, group by normal/effect/etc.
				if (extra1 != extra2)
					return extra1 < extra2;
				// then level, atk, def
				if (data1->level != data2->level)
					return data1->level < data2->level;
				if (data1->attack != data2->attack)
					return data1->attack < data2->attack;
				if (data1->defense != data2->defense)
					return data1->defense < data2->defense;
			}
			// finally fall back to card code & sequence
			if (e1->code != e2->code)
				return e1->code < e2->code;
			return e1->sequence > e2->sequence;
		}
	} else if (e1->location & (LOCATION_GRAVE | LOCATION_REMOVED)) {
		// any cards involved in ongoing chain links are sorted to the top
		auto chainOrder = [](ClientCard* c) {
			for (size_t i = 0; i < mainGame->dField.chains.size(); ++i) {
				ChainInfo const& it = mainGame->dField.chains[i];
				// card whose effect was activated is sorted...
				if (c == it.chain_card)
					return (i * 2) + 1;
				// ...before the chain link's target
				if (it.target.find(c) != it.target.end())
					return (i * 2) + 2;
			}
			return size_t{ 0u };
		};
		size_t const o1 = chainOrder(e1);
		size_t const o2 = chainOrder(e2);
		// more recent chain links come first
		if (o1 != o2)
			return o1 > o2;
		// GY/banish cards are shown reversed (highest index first)
		return e1->sequence > e2->sequence;
	} else {
		// other locations (field)
		return e1->sequence < e2->sequence;
	}
}

}
