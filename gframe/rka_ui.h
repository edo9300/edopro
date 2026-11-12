#ifndef RKA_UI_H
#define RKA_UI_H
// Screens added by this fork: deck optimiser, hand test setup and save states.
// The implementation lives in rka_ui.inl, compiled as part of deck_con.cpp so
// that builds with an explicit source list need no changes.

#include <IEventReceiver.h>

namespace ygo {

class RkaUI {
public:
	// builds every window; called once while the game creates its GUI
	static void Create();
	static void OnResize();
	// deck editor: buttons of this fork follow the editor's own buttons
	static void ShowDeckButtons(bool visible);
	static bool OnDeckEvent(const irr::SEvent& event);
	// duel: save state / load state / take over
	static void ShowDuelButtons(bool visible);
	static void HideDuelWindows();
	static bool OnDuelEvent(const irr::SEvent& event);
	static void StartHandTest();
};

}

#endif //RKA_UI_H
