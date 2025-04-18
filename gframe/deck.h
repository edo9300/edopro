#ifndef DECK_H
#define DECK_H
#include <vector>

namespace ygo {

struct CardDataC;
struct Deck {
	using Vector = std::vector<const CardDataC*>;
	Vector main;
	Vector extra;
	Vector side;
	void clear() {
		main.clear();
		extra.clear();
		side.clear();
	}
};
enum class RITUAL_LOCATION : uint8_t {
	DEFAULT,
	MAIN,
	EXTRA,
};
}

#endif //DECK_H
