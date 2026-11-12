// Implementation of RkaUI. Included at the end of deck_con.cpp (see rka_ui.h).
// Every window here is laid out for touch first: large rows, one tap per
// action, and the windows grow with the screen instead of staying desktop size.

#include <algorithm>
#include <ctime>
#include <IGUIButton.h>
#include <IGUICheckBox.h>
#include <IGUIComboBox.h>
#include <IGUIEnvironment.h>
#include <IGUIListBox.h>
#include <IGUIStaticText.h>
#include <IGUIWindow.h>
#include "deck_optimizer.h"
#include "handtrap_ai.h"
#include "replay_mode.h"
#include "rka_ui.h"
#include "single_mode.h"
#if EDOPRO_ANDROID || EDOPRO_IOS
#include "CGUICustomComboBox/CGUICustomComboBox.h"
#define RkaAddComboBox(env, ...) irr::gui::CGUICustomComboBox::addCustomComboBox(env, __VA_ARGS__)
#else
#define RkaAddComboBox(env, ...) env->addComboBox(__VA_ARGS__)
#endif

namespace ygo {

namespace {

using irr::gui::IGUIButton;
using irr::gui::IGUIComboBox;
using irr::gui::IGUIElement;
using irr::gui::IGUIListBox;
using irr::gui::IGUIStaticText;
using irr::gui::IGUIWindow;

enum OpponentMode {
	OPPONENT_NONE,
	OPPONENT_MIRROR,
	OPPONENT_EASY,
	OPPONENT_SMART,
};

struct BigWindow {
	IGUIWindow* window{ nullptr };
	int width{ 0 };
	int height{ 0 };
};

struct {
	// deck editor
	IGUIButton* btnOptimizer{ nullptr };
	// optimiser
	BigWindow optimizer;
	IGUIStaticText* stCounts{ nullptr };
	IGUIListBox* lstCards{ nullptr };
	IGUIButton* btnRole[rka::ROLE_COUNT]{};
	IGUIStaticText* stHelp{ nullptr };
	IGUIButton* btnFirst{ nullptr };
	IGUIButton* btnSecond{ nullptr };
	IGUIStaticText* stOdds{ nullptr };
	IGUIStaticText* stAdvice{ nullptr };
	rka::DeckAnalysis analysis;
	bool going_second{ false };
	// hand test setup
	IGUIComboBox* cbHandSize{ nullptr };
	IGUIComboBox* cbHandMode{ nullptr };
	IGUIButton* btnPickHand{ nullptr };
	IGUIStaticText* stHandSummary{ nullptr };
	IGUIComboBox* cbOpponent{ nullptr };
	IGUIStaticText* stOpponentHelp{ nullptr };
	IGUIStaticText* stTrapCount{ nullptr };
	IGUIComboBox* cbTrapCount{ nullptr };
	IGUIButton* btnPickTraps{ nullptr };
	std::vector<uint32_t> chosen_hand;
	std::vector<uint32_t> trap_pool;
	// opening hand picker
	BigWindow handPick;
	IGUIListBox* lstPickDeck{ nullptr };
	IGUIListBox* lstPickHand{ nullptr };
	IGUIStaticText* stPickHand{ nullptr };
	std::vector<uint32_t> pick_deck_codes; // row -> card
	// hand trap picker
	BigWindow trapPick;
	IGUIListBox* lstTraps{ nullptr };
	std::vector<uint32_t> trap_rows; // row -> card
	// save states
	BigWindow states;
	IGUIListBox* lstStates{ nullptr };
	std::vector<epro::path_string> state_files;
	IGUIButton* btnSaveState{ nullptr };
	IGUIButton* btnLoadState{ nullptr };
	IGUIButton* btnTakeOver{ nullptr };
} ui;

template<typename T>
T Aligned(T elem) {
	elem->setAlignment(irr::gui::EGUIA_SCALE, irr::gui::EGUIA_SCALE, irr::gui::EGUIA_SCALE, irr::gui::EGUIA_SCALE);
	return elem;
}

irr::core::recti R(int x, int y, int x2, int y2) {
	return mainGame->Scale(x, y, x2, y2);
}

// Skins draw windows see-through, which makes long lists hard to read over the deck or the field.
void Backdrop(IGUIElement* window, int width, int height) {
	auto* backdrop = Aligned(mainGame->env->addStaticText(L"", R(3, 22, width - 3, height - 3), false, false, window, -1, true));
	backdrop->setBackgroundColor(irr::video::SColor(238, 14, 14, 20));
}

// A tap can reach a list as more than one event (press, release, repeat):
// only the first one within a short moment counts.
bool IsNewTap(int list_id, int row) {
	static int last_list = -1;
	static int last_row = -1;
	static irr::u32 last_time = 0;
	const auto now = mainGame->device->getTimer()->getRealTime();
	const bool repeated = list_id == last_list && row == last_row && now - last_time < 350;
	last_list = list_id;
	last_row = row;
	last_time = now;
	return !repeated;
}

BigWindow MakeWindow(int width, int height, const wchar_t* title) {
	BigWindow big;
	big.width = width;
	big.height = height;
	big.window = mainGame->env->addWindow(R(0, 0, width, height), false, title);
	big.window->getCloseButton()->setVisible(false);
	big.window->setVisible(false);
	Backdrop(big.window, width, height);
	return big;
}

IGUIButton* Button(const BigWindow& parent, int x, int y, int x2, int y2, int id, const wchar_t* text) {
	return Aligned(mainGame->env->addButton(R(x, y, x2, y2), parent.window, id, text));
}

IGUIStaticText* Label(IGUIElement* parent, int x, int y, int x2, int y2, const wchar_t* text, bool wrap = false, bool border = false) {
	auto* label = Aligned(mainGame->env->addStaticText(text, R(x, y, x2, y2), border, wrap, parent));
	label->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, wrap ? irr::gui::EGUIA_UPPERLEFT : irr::gui::EGUIA_CENTER);
	return label;
}

IGUIListBox* List(const BigWindow& parent, int x, int y, int x2, int y2, int id) {
	auto* list = Aligned(mainGame->env->addListBox(R(x, y, x2, y2), parent.window, id, true));
	// rows tall enough for a thumb
	list->setItemHeight(mainGame->Scale(30));
	return list;
}

// Windows keep their designed size on a desktop and fill most of a phone screen.
void Place(const BigWindow& big) {
	if(!big.window)
		return;
	const int screen_w = static_cast<int>(mainGame->window_size.Width);
	const int screen_h = static_cast<int>(mainGame->window_size.Height);
	const float aspect = static_cast<float>(big.width) / static_cast<float>(big.height);
	int height = std::max(mainGame->Scale(big.height), static_cast<int>(screen_h * 0.92f));
	height = std::min(height, screen_h);
	int width = static_cast<int>(height * aspect);
	if(width > screen_w) {
		width = screen_w;
		height = static_cast<int>(width / aspect);
	}
	const int x = (screen_w - width) / 2;
	const int y = (screen_h - height) / 2;
	big.window->setRelativePosition(irr::core::recti(x, y, x + width, y + height));
}

void Open(const BigWindow& big) {
	Place(big);
	big.window->getParent()->bringToFront(big.window);
	mainGame->ShowElement(big.window);
}

void Close(const BigWindow& big) {
	if(big.window && big.window->isVisible())
		mainGame->HideElement(big.window);
}

void Toast(epro::wstringview text) {
	mainGame->stACMessage->setText(text.data());
	mainGame->wACMessage->getParent()->bringToFront(mainGame->wACMessage);
	mainGame->ShowElement(mainGame->wACMessage, 90);
}

std::wstring CardName(uint32_t code) {
	const auto name = gDataManager->GetName(code);
	if(name.empty())
		return epro::format(L"??? ({})", code);
	return std::wstring{ name.data(), name.size() };
}

std::string CurrentDeckName() {
	const auto selected = mainGame->cbDBDecks->getSelected();
	const wchar_t* name = selected >= 0 ? mainGame->cbDBDecks->getItem(selected) : mainGame->ebDeckname->getText();
	return BufferIO::EncodeUTF8(name ? name : L"");
}

irr::video::SColor RoleColor(rka::Role role) {
	switch(role) {
	case rka::ROLE_STARTER: return irr::video::SColor(255, 110, 230, 120);
	case rka::ROLE_EXTENDER: return irr::video::SColor(255, 100, 215, 230);
	case rka::ROLE_ENGINE_REQ: return irr::video::SColor(255, 255, 170, 80);
	case rka::ROLE_HANDTRAP: return irr::video::SColor(255, 200, 150, 255);
	case rka::ROLE_BREAKER: return irr::video::SColor(255, 255, 120, 120);
	case rka::ROLE_BRICK: return irr::video::SColor(255, 170, 170, 170);
	case rka::ROLE_TECH: return irr::video::SColor(255, 235, 235, 235);
	default: return irr::video::SColor(255, 255, 225, 90);
	}
}

/////////////////////////////////////////////////////////////////////////////
// Deck optimiser

void RefreshOptimizer(uint32_t keep_selected = 0) {
	if(!keep_selected) {
		const auto selected = ui.lstCards->getSelected();
		if(selected >= 0 && static_cast<size_t>(selected) < ui.analysis.entries.size())
			keep_selected = ui.analysis.entries[selected].code;
	}
	ui.analysis = rka::AnalyzeDeck(mainGame->deckBuilder.GetCurrentDeck(), CurrentDeckName());
	const auto scroll = ui.lstCards->getVerticalScrollBar() ? ui.lstCards->getVerticalScrollBar()->getPos() : 0;
	ui.lstCards->clear();
	int select = -1;
	for(const auto& entry : ui.analysis.entries) {
		const auto row = ui.lstCards->addItem(epro::format(L"{}x  {}   [{}{}]", entry.count, rka::EntryName(entry), rka::RoleName(entry.role),
														 entry.guessed ? L", guessed" : L"").data());
		ui.lstCards->setItemOverrideColor(row, RoleColor(entry.role));
		if(entry.code == keep_selected)
			select = static_cast<int>(row);
	}
	ui.lstCards->setSelected(select);
	if(ui.lstCards->getVerticalScrollBar())
		ui.lstCards->getVerticalScrollBar()->setPos(scroll);
	ui.stCounts->setText(rka::FormatCounts(ui.analysis).data());
	ui.stOdds->setText(rka::FormatOdds(ui.analysis, ui.going_second).data());
	ui.stAdvice->setText(rka::FormatAdvice(ui.analysis).data());
	ui.btnFirst->setPressed(!ui.going_second);
	ui.btnSecond->setPressed(ui.going_second);
	const rka::DeckEntry* entry = select >= 0 ? &ui.analysis.entries[select] : nullptr;
	for(int role = 0; role < rka::ROLE_COUNT; ++role)
		ui.btnRole[role]->setPressed(entry && entry->role == role && !(role == rka::ROLE_NONE));
	if(entry)
		ui.stHelp->setText(epro::format(L"{}: {}", rka::RoleName(entry->role), rka::RoleHelp(entry->role)).data());
	else
		ui.stHelp->setText(L"Tap a card on the left, then tap what it does in your deck.");
}

void AssignRole(rka::Role role) {
	const auto selected = ui.lstCards->getSelected();
	if(selected < 0 || static_cast<size_t>(selected) >= ui.analysis.entries.size()) {
		ui.stHelp->setText(epro::format(L"{}: {}\nTap a card on the left first.", rka::RoleName(role), rka::RoleHelp(role)).data());
		for(int i = 0; i < rka::ROLE_COUNT; ++i)
			ui.btnRole[i]->setPressed(false);
		return;
	}
	const auto code = ui.analysis.entries[selected].code;
	rka::RoleStore::Get().Assign(CurrentDeckName(), code, role);
	// move on to the next card that still needs a tag, so tagging a deck is tap, tap, tap
	uint32_t next = code;
	for(size_t i = selected + 1; i < ui.analysis.entries.size(); ++i) {
		if(ui.analysis.entries[i].role == rka::ROLE_NONE) {
			next = ui.analysis.entries[i].code;
			break;
		}
	}
	RefreshOptimizer(next);
	const auto now_selected = ui.lstCards->getSelected();
	if(now_selected >= 0)
		mainGame->ShowCardInfo(ui.analysis.entries[now_selected].card->code);
}

void CreateOptimizer() {
	auto* env = mainGame->env;
	ui.optimizer = MakeWindow(820, 600, L"Deck Optimiser");
	auto* window = ui.optimizer.window;
	ui.stCounts = Label(window, 10, 26, 810, 48, L"");
	ui.lstCards = List(ui.optimizer, 10, 52, 400, 420, LISTBOX_RKA_OPT_CARDS);
	static constexpr rka::Role order[] = { rka::ROLE_STARTER, rka::ROLE_EXTENDER, rka::ROLE_ENGINE_REQ, rka::ROLE_HANDTRAP,
										   rka::ROLE_BREAKER, rka::ROLE_BRICK, rka::ROLE_TECH, rka::ROLE_NONE };
	for(int i = 0; i < 8; ++i) {
		const auto role = order[i];
		const int x = (i % 2) ? 615 : 410;
		const int y = 52 + (i / 2) * 44;
		auto* button = Aligned(env->addButton(R(x, y, x + 195, y + 38), window, BUTTON_RKA_ROLE_FIRST + role,
											   role == rka::ROLE_NONE ? L"Clear tag" : rka::RoleName(role)));
		button->setIsPushButton(true);
		ui.btnRole[role] = button;
	}
	ui.stHelp = Label(window, 410, 230, 810, 274, L"", true);
	ui.btnFirst = Aligned(env->addButton(R(410, 278, 605, 312), window, BUTTON_RKA_OPT_FIRST, L"Going first (5 cards)"));
	ui.btnFirst->setIsPushButton(true);
	ui.btnSecond = Aligned(env->addButton(R(615, 278, 810, 312), window, BUTTON_RKA_OPT_SECOND, L"Going second (6 cards)"));
	ui.btnSecond->setIsPushButton(true);
	ui.stOdds = Label(window, 410, 318, 810, 420, L"", true);
	ui.stAdvice = Label(window, 10, 428, 810, 552, L"", true, true);
	Label(window, 10, 558, 630, 592, L"Opening hand odds are exact, not simulated. Tags are saved with the deck.");
	Button(ui.optimizer, 640, 558, 810, 592, BUTTON_RKA_OPT_CLOSE, L"Done");
}

/////////////////////////////////////////////////////////////////////////////
// Hand test setup

int HandSize() {
	return std::clamp(ui.cbHandSize->getSelected() + 1, 1, 6);
}

int CopiesInDeck(uint32_t code) {
	int copies = 0;
	for(const auto* card : mainGame->deckBuilder.GetCurrentDeck().main)
		copies += card->code == code;
	return copies;
}

// drops picks that no longer fit the deck or the hand size
void ValidateChosenHand() {
	std::vector<uint32_t> valid;
	for(auto code : ui.chosen_hand) {
		if(static_cast<int>(valid.size()) >= HandSize())
			break;
		if(std::count(valid.begin(), valid.end(), code) < CopiesInDeck(code))
			valid.push_back(code);
	}
	ui.chosen_hand = std::move(valid);
}

void RefreshHandTest() {
	ValidateChosenHand();
	const bool choose = ui.cbHandMode->getSelected() == 1;
	const int hand = HandSize();
	ui.btnPickHand->setEnabled(choose);
	if(!choose)
		ui.stHandSummary->setText(epro::format(L"You draw {} random card{}.", hand, hand == 1 ? L"" : L"s").data());
	else if(ui.chosen_hand.empty())
		ui.stHandSummary->setText(L"No cards picked yet. Tap \"Pick cards\".");
	else {
		std::wstring text;
		for(auto code : ui.chosen_hand) {
			if(!text.empty())
				text += L", ";
			text += CardName(code);
		}
		const int random = hand - static_cast<int>(ui.chosen_hand.size());
		if(random > 0)
			text += epro::format(L"  + {} random", random);
		ui.stHandSummary->setText(text.data());
	}
	const auto mode = ui.cbOpponent->getSelected();
	const bool traps = mode == OPPONENT_EASY || mode == OPPONENT_SMART;
	switch(mode) {
	case OPPONENT_MIRROR:
		ui.stOpponentHelp->setText(L"The opponent uses a copy of your deck and you make every choice for both sides.");
		break;
	case OPPONENT_EASY:
		ui.stOpponentHelp->setText(L"The opponent holds hand traps and uses each one at the first chance it gets, like Ash Blossom on your first search.");
		break;
	case OPPONENT_SMART:
		ui.stOpponentHelp->setText(L"The opponent holds hand traps and saves them for your starters and key cards, as tagged in the Deck Optimiser.");
		break;
	default:
		ui.stOpponentHelp->setText(L"Nobody on the other side. Just you and your combo.");
		break;
	}
	ui.stTrapCount->setEnabled(traps);
	ui.cbTrapCount->setEnabled(traps);
	ui.btnPickTraps->setEnabled(traps);
	ui.btnPickTraps->setText(epro::format(L"Choose hand traps ({})", ui.trap_pool.size()).data());
}

void RefreshHandPicker() {
	ValidateChosenHand();
	ui.lstPickDeck->clear();
	ui.pick_deck_codes.clear();
	for(const auto* card : mainGame->deckBuilder.GetCurrentDeck().main) {
		if(std::find(ui.pick_deck_codes.begin(), ui.pick_deck_codes.end(), card->code) != ui.pick_deck_codes.end())
			continue;
		ui.pick_deck_codes.push_back(card->code);
		const auto copies = CopiesInDeck(card->code);
		const auto used = std::count(ui.chosen_hand.begin(), ui.chosen_hand.end(), card->code);
		const auto row = ui.lstPickDeck->addItem(epro::format(L"{} / {}   {}", used, copies, CardName(card->code)).data());
		if(used)
			ui.lstPickDeck->setItemOverrideColor(row, RoleColor(rka::ROLE_STARTER));
	}
	ui.lstPickHand->clear();
	for(auto code : ui.chosen_hand)
		ui.lstPickHand->addItem(CardName(code).data());
	ui.stPickHand->setText(epro::format(L"Opening hand ({} of {})  -  tap to remove", ui.chosen_hand.size(), HandSize()).data());
}

void RefreshTrapPicker() {
	const auto scroll = ui.lstTraps->getVerticalScrollBar() ? ui.lstTraps->getVerticalScrollBar()->getPos() : 0;
	ui.lstTraps->clear();
	ui.trap_rows.clear();
	for(const auto& known : rka::KnownHandtraps()) {
		if(!gDataManager->GetCardData(known.code))
			continue;
		ui.trap_rows.push_back(known.code);
		const bool on = std::find(ui.trap_pool.begin(), ui.trap_pool.end(), known.code) != ui.trap_pool.end();
		const auto row = ui.lstTraps->addItem(epro::format(L"{}   {}", on ? L"[ON]" : L"[  -  ]", CardName(known.code)).data());
		if(on)
			ui.lstTraps->setItemOverrideColor(row, RoleColor(rka::ROLE_HANDTRAP));
	}
	if(ui.lstTraps->getVerticalScrollBar())
		ui.lstTraps->getVerticalScrollBar()->setPos(scroll);
}

void CreateHandTest() {
	auto* env = mainGame->env;
	const int width = 520;
	mainGame->wHandTest = env->addWindow(R(0, 0, width, 440), false, L"Hand Test");
	auto* window = mainGame->wHandTest;
	window->getCloseButton()->setVisible(false);
	window->setVisible(false);
	Backdrop(window, width, 440);
	auto& store = rka::RoleStore::Get();
	ui.trap_pool = store.GetHandtrapPool();

	Label(window, 10, 30, 200, 62, L"Hand size");
	ui.cbHandSize = RkaAddComboBox(env, R(210, 30, width - 10, 62), window, COMBOBOX_RKA_HT_HANDSIZE);
	for(int i = 1; i <= 6; ++i)
		ui.cbHandSize->addItem(epro::format(L"{} card{}{}", i, i == 1 ? L"" : L"s", i == 5 ? L"  (going first)" : i == 6 ? L"  (going second)" : L"").data());
	ui.cbHandSize->setSelected(std::clamp(store.GetSetting("hand_size", 5), 1, 6) - 1);

	Label(window, 10, 70, 200, 102, L"Opening hand");
	ui.cbHandMode = RkaAddComboBox(env, R(210, 70, 360, 102), window, COMBOBOX_RKA_HT_HANDMODE);
	ui.cbHandMode->addItem(L"Random");
	ui.cbHandMode->addItem(L"I choose");
	ui.btnPickHand = env->addButton(R(370, 70, width - 10, 102), window, BUTTON_RKA_HT_PICK_HAND, L"Pick cards");
	ui.stHandSummary = Label(window, 10, 106, width - 10, 142, L"", true);

	Label(window, 10, 150, 200, 182, L"Opponent");
	ui.cbOpponent = RkaAddComboBox(env, R(210, 150, width - 10, 182), window, COMBOBOX_RKA_HT_OPPONENT);
	ui.cbOpponent->addItem(L"No opponent");
	ui.cbOpponent->addItem(L"Copy of my deck (I play both)");
	ui.cbOpponent->addItem(L"Hand traps - Easy");
	ui.cbOpponent->addItem(L"Hand traps - Smart");
	ui.cbOpponent->setSelected(std::clamp(store.GetSetting("opponent", OPPONENT_NONE), 0, 3));
	ui.stOpponentHelp = Label(window, 10, 186, width - 10, 224, L"", true);

	ui.stTrapCount = Label(window, 10, 230, 200, 262, L"Hand traps held");
	ui.cbTrapCount = RkaAddComboBox(env, R(210, 230, 270, 262), window, COMBOBOX_RKA_HT_TRAPCOUNT);
	for(int i = 1; i <= 5; ++i)
		ui.cbTrapCount->addItem(epro::to_wstring(i).data());
	ui.cbTrapCount->setSelected(std::clamp(store.GetSetting("trap_count", 2), 1, 5) - 1);
	ui.btnPickTraps = env->addButton(R(280, 230, width - 10, 262), window, BUTTON_RKA_HT_PICK_TRAPS, L"");

	mainGame->chkHandTestNoShuffle = env->addCheckBox(false, R(10, 270, width - 10, 298), window, -1, gDataManager->GetSysString(1230).data());
	mainGame->defaultStrings.emplace_back(mainGame->chkHandTestNoShuffle, 1230);
	auto* rules = Label(window, 10, 304, 200, 336, gDataManager->GetSysString(1236).data());
	mainGame->defaultStrings.emplace_back(rules, 1236);
	mainGame->cbHandTestDuelRule = RkaAddComboBox(env, R(210, 304, width - 10, 336), window);
	mainGame->ReloadCBDuelRule(mainGame->cbHandTestDuelRule);
	mainGame->cbHandTestDuelRule->setSelected(4);
	mainGame->chkHandTestSaveReplay = env->addCheckBox(gGameConfig->saveHandTest, R(10, 342, width - 10, 370), window, CHECKBOX_SAVE_HAND_TEST_REPLAY, gDataManager->GetSysString(2077).data());
	mainGame->defaultStrings.emplace_back(mainGame->chkHandTestSaveReplay, 2077);

	env->addButton(R(10, 384, 170, 424), window, BUTTON_RKA_HT_STATES, L"Saved states");
	env->addButton(R(180, 384, 340, 424), window, BUTTON_HAND_TEST_CANCEL, L"Cancel");
	auto* start = env->addButton(R(350, 384, width - 10, 424), window, BUTTON_HAND_TEST_START, gDataManager->GetSysString(1215).data());
	mainGame->defaultStrings.emplace_back(start, 1215);

	ui.handPick = MakeWindow(700, 520, L"Pick your opening hand");
	Label(ui.handPick.window, 10, 26, 345, 50, L"Your deck  -  tap to add");
	ui.stPickHand = Label(ui.handPick.window, 355, 26, 690, 50, L"");
	ui.lstPickDeck = List(ui.handPick, 10, 52, 345, 462, LISTBOX_RKA_PICK_DECK);
	ui.lstPickHand = List(ui.handPick, 355, 52, 690, 462, LISTBOX_RKA_PICK_HAND);
	Button(ui.handPick, 10, 470, 170, 508, BUTTON_RKA_PICK_CLEAR, L"Clear");
	Label(ui.handPick.window, 180, 470, 520, 508, L"Empty slots are filled with random cards.");
	Button(ui.handPick, 530, 470, 690, 508, BUTTON_RKA_PICK_DONE, L"Done");

	ui.trapPick = MakeWindow(480, 520, L"Hand traps the opponent can hold");
	Label(ui.trapPick.window, 10, 26, 470, 50, L"Tap to switch a card on or off.");
	ui.lstTraps = List(ui.trapPick, 10, 52, 470, 462, LISTBOX_RKA_TRAPS);
	Button(ui.trapPick, 310, 470, 470, 508, BUTTON_RKA_TRAPS_DONE, L"Done");
}

uint64_t SelectedDuelRule() {
	switch(mainGame->cbHandTestDuelRule->getSelected()) {
	case 0: return DUEL_MODE_MR1;
	case 1: return DUEL_MODE_MR2;
	case 2: return DUEL_MODE_MR3;
	case 3: return DUEL_MODE_MR4;
	case 5: return DUEL_MODE_SPEED;
	case 6: return DUEL_MODE_RUSH;
	case 7: return DUEL_MODE_GOAT;
	default: return DUEL_MODE_MR5;
	}
}

/////////////////////////////////////////////////////////////////////////////
// Save states

std::wstring StateLabel(const epro::path_string& file) {
	auto label = Utils::ToUnicodeIfNeeded(file);
	if(label.size() > 4)
		label.resize(label.size() - 4); // ".yrp"
	return label;
}

void RefreshStates() {
	ui.state_files = SingleMode::ListStates();
	ui.lstStates->clear();
	for(const auto& file : ui.state_files)
		ui.lstStates->addItem(StateLabel(file).data());
	if(!ui.state_files.empty())
		ui.lstStates->setSelected(0);
}

void OpenStates() {
	RefreshStates();
	Open(ui.states);
}

void LoadSelectedState() {
	const auto selected = ui.lstStates->getSelected();
	if(selected < 0 || static_cast<size_t>(selected) >= ui.state_files.size())
		return;
	const auto file = ui.state_files[selected];
	Close(ui.states);
	if(mainGame->dInfo.isSingleMode) {
		if(!SingleMode::LoadState(file))
			Toast(L"That state could not be loaded.");
		return;
	}
	// from the deck editor: the duel replaces the editor until it is closed
	mainGame->HideElement(mainGame->wHandTest);
	mainGame->deckBuilder.Terminate(false);
	if(!SingleMode::LoadState(file, SingleMode::DuelOptions::RETURN_DECK_EDIT)) {
		mainGame->deckBuilder.Initialize(false);
		Toast(L"That state could not be loaded.");
	}
}

void CreateStates() {
	auto* env = mainGame->env;
	ui.states = MakeWindow(560, 420, L"Saved states");
	ui.lstStates = List(ui.states, 10, 30, 550, 362, LISTBOX_RKA_STATES);
	Button(ui.states, 10, 370, 180, 408, BUTTON_RKA_STATE_LOAD, L"Load");
	Button(ui.states, 195, 370, 365, 408, BUTTON_RKA_STATE_DELETE, L"Delete");
	Button(ui.states, 380, 370, 550, 408, BUTTON_RKA_STATE_CLOSE, L"Close");
	ui.btnSaveState = Aligned(env->addButton(R(205, 95, 295, 130), nullptr, BUTTON_RKA_SAVE_STATE, L"Save state"));
	ui.btnSaveState->setVisible(false);
	ui.btnLoadState = Aligned(env->addButton(R(205, 135, 295, 170), nullptr, BUTTON_RKA_LOAD_STATE, L"Load state"));
	ui.btnLoadState->setVisible(false);
}

bool HandleStateEvent(int id, irr::gui::EGUI_EVENT_TYPE type) {
	if(type == irr::gui::EGET_BUTTON_CLICKED) {
		switch(id) {
		case BUTTON_RKA_STATE_LOAD:
			LoadSelectedState();
			return true;
		case BUTTON_RKA_STATE_DELETE: {
			const auto selected = ui.lstStates->getSelected();
			if(selected >= 0 && static_cast<size_t>(selected) < ui.state_files.size()) {
				SingleMode::DeleteState(ui.state_files[selected]);
				RefreshStates();
			}
			return true;
		}
		case BUTTON_RKA_STATE_CLOSE:
			Close(ui.states);
			return true;
		default:
			return false;
		}
	}
	if(type == irr::gui::EGET_LISTBOX_SELECTED_AGAIN && id == LISTBOX_RKA_STATES) {
		LoadSelectedState();
		return true;
	}
	return false;
}

}

/////////////////////////////////////////////////////////////////////////////

void RkaUI::Create() {
	auto* env = mainGame->env;
	ui.btnOptimizer = Aligned(env->addButton(R(205, 236, 295, 271), nullptr, BUTTON_RKA_OPTIMIZER, L"Optimise"));
	ui.btnOptimizer->setVisible(false);
	CreateOptimizer();
	CreateHandTest();
	CreateStates();
	// replay playback: stop watching and start playing
	ui.btnTakeOver = Aligned(env->addButton(R(5, 130, 85, 150), mainGame->wReplayControl, BUTTON_REPLAY_TAKEOVER, L"Take over"));
	RefreshHandTest();
}

void RkaUI::OnResize() {
	Place(ui.optimizer);
	Place(ui.handPick);
	Place(ui.trapPick);
	Place(ui.states);
}

void RkaUI::ShowDeckButtons(bool visible) {
	if(!ui.btnOptimizer)
		return;
	ui.btnOptimizer->setVisible(visible);
	if(!visible) {
		ui.optimizer.window->setVisible(false);
		ui.handPick.window->setVisible(false);
		ui.trapPick.window->setVisible(false);
		ui.states.window->setVisible(false);
	}
}

void RkaUI::ShowDuelButtons(bool visible) {
	if(!ui.btnSaveState)
		return;
	ui.btnSaveState->setVisible(visible);
	ui.btnLoadState->setVisible(visible);
	if(!visible)
		ui.states.window->setVisible(false);
}

void RkaUI::HideDuelWindows() {
	if(ui.states.window)
		ui.states.window->setVisible(false);
}

void RkaUI::StartHandTest() {
	auto& store = rka::RoleStore::Get();
	ValidateChosenHand();
	SingleMode::DuelOptions options("hand-test-mode");
	options.startingDrawCount = static_cast<uint32_t>(HandSize());
	options.duelFlags = SelectedDuelRule();
	options.duelFlags |= mainGame->chkHandTestNoShuffle->isChecked() ? DUEL_PSEUDO_SHUFFLE : 0;
	if(ui.cbHandMode->getSelected() == 1)
		options.startingHand = ui.chosen_hand;
	options.deckName = CurrentDeckName();
	const auto mode = ui.cbOpponent->getSelected();
	options.handTestNoOpponent = mode != OPPONENT_MIRROR;
	if(mode == OPPONENT_EASY || mode == OPPONENT_SMART) {
		// up to three copies of every allowed hand trap, shuffled, and the opponent keeps the top few
		std::vector<uint32_t> pool;
		for(auto code : ui.trap_pool) {
			if(gDataManager->GetCardData(code))
				pool.insert(pool.end(), 3, code);
		}
		std::shuffle(pool.begin(), pool.end(), Utils::GetRandomNumberGenerator());
		const auto held = std::min<size_t>(pool.size(), static_cast<size_t>(ui.cbTrapCount->getSelected() + 1));
		options.opponentHand.assign(pool.begin(), pool.begin() + held);
		if(!options.opponentHand.empty())
			options.aiMode = mode == OPPONENT_SMART ? rka::HandtrapAI::SMART : rka::HandtrapAI::EASY;
	}
	store.SetSetting("hand_size", HandSize());
	store.SetSetting("opponent", mode);
	store.SetSetting("trap_count", ui.cbTrapCount->getSelected() + 1);
	mainGame->deckBuilder.Terminate(false);
	SingleMode::singleSignal.SetNoWait(false);
	SingleMode::StartPlay(std::move(options));
}

bool RkaUI::OnDeckEvent(const irr::SEvent& event) {
	if(event.EventType != irr::EET_GUI_EVENT || !ui.btnOptimizer)
		return false;
	const int id = event.GUIEvent.Caller->getID();
	const auto type = event.GUIEvent.EventType;
	if(HandleStateEvent(id, type))
		return true;
	switch(type) {
	case irr::gui::EGET_BUTTON_CLICKED: {
		if(id >= BUTTON_RKA_ROLE_FIRST && id < BUTTON_RKA_ROLE_FIRST + rka::ROLE_COUNT) {
			AssignRole(static_cast<rka::Role>(id - BUTTON_RKA_ROLE_FIRST));
			return true;
		}
		switch(id) {
		case BUTTON_RKA_OPTIMIZER:
			ui.lstCards->setSelected(-1);
			RefreshOptimizer();
			Open(ui.optimizer);
			return true;
		case BUTTON_RKA_OPT_CLOSE:
			Close(ui.optimizer);
			return true;
		case BUTTON_RKA_OPT_FIRST:
		case BUTTON_RKA_OPT_SECOND:
			ui.going_second = id == BUTTON_RKA_OPT_SECOND;
			RefreshOptimizer();
			return true;
		case BUTTON_HAND_TEST_SETTINGS:
			RefreshHandTest();
			return false; // the editor still opens the window itself
		case BUTTON_RKA_HT_PICK_HAND:
			RefreshHandPicker();
			Open(ui.handPick);
			return true;
		case BUTTON_RKA_PICK_CLEAR:
			ui.chosen_hand.clear();
			RefreshHandPicker();
			return true;
		case BUTTON_RKA_PICK_DONE:
			Close(ui.handPick);
			RefreshHandTest();
			return true;
		case BUTTON_RKA_HT_PICK_TRAPS:
			RefreshTrapPicker();
			Open(ui.trapPick);
			return true;
		case BUTTON_RKA_TRAPS_DONE:
			Close(ui.trapPick);
			rka::RoleStore::Get().SetHandtrapPool(ui.trap_pool);
			RefreshHandTest();
			return true;
		case BUTTON_RKA_HT_STATES:
			OpenStates();
			return true;
		default:
			return false;
		}
	}
	case irr::gui::EGET_COMBO_BOX_CHANGED: {
		if(id == COMBOBOX_RKA_HT_HANDSIZE || id == COMBOBOX_RKA_HT_HANDMODE || id == COMBOBOX_RKA_HT_OPPONENT) {
			RefreshHandTest();
			return true;
		}
		return id == COMBOBOX_RKA_HT_TRAPCOUNT;
	}
	case irr::gui::EGET_LISTBOX_CHANGED:
	case irr::gui::EGET_LISTBOX_SELECTED_AGAIN: {
		auto* list = static_cast<IGUIListBox*>(event.GUIEvent.Caller);
		const auto selected = list->getSelected();
		switch(id) {
		case LISTBOX_RKA_OPT_CARDS: {
			if(selected >= 0 && static_cast<size_t>(selected) < ui.analysis.entries.size()) {
				const auto& entry = ui.analysis.entries[selected];
				mainGame->ShowCardInfo(entry.card->code);
				for(int role = 0; role < rka::ROLE_COUNT; ++role)
					ui.btnRole[role]->setPressed(entry.role == role && role != rka::ROLE_NONE);
				ui.stHelp->setText(epro::format(L"{}: {}", rka::RoleName(entry.role), rka::RoleHelp(entry.role)).data());
			}
			return true;
		}
		case LISTBOX_RKA_PICK_DECK: {
			if(selected >= 0 && static_cast<size_t>(selected) < ui.pick_deck_codes.size() && IsNewTap(id, selected)) {
				const auto code = ui.pick_deck_codes[selected];
				mainGame->ShowCardInfo(code);
				if(static_cast<int>(ui.chosen_hand.size()) < HandSize() &&
				   std::count(ui.chosen_hand.begin(), ui.chosen_hand.end(), code) < CopiesInDeck(code))
					ui.chosen_hand.push_back(code);
				const auto scroll = list->getVerticalScrollBar() ? list->getVerticalScrollBar()->getPos() : 0;
				RefreshHandPicker();
				// nothing stays highlighted, so a second tap on the same row adds a second copy
				if(list->getVerticalScrollBar())
					list->getVerticalScrollBar()->setPos(scroll);
			}
			return true;
		}
		case LISTBOX_RKA_PICK_HAND: {
			if(selected >= 0 && static_cast<size_t>(selected) < ui.chosen_hand.size() && IsNewTap(id, selected)) {
				ui.chosen_hand.erase(ui.chosen_hand.begin() + selected);
				RefreshHandPicker();
			}
			return true;
		}
		case LISTBOX_RKA_TRAPS: {
			if(selected >= 0 && static_cast<size_t>(selected) < ui.trap_rows.size() && IsNewTap(id, selected)) {
				const auto code = ui.trap_rows[selected];
				mainGame->ShowCardInfo(code);
				auto it = std::find(ui.trap_pool.begin(), ui.trap_pool.end(), code);
				if(it == ui.trap_pool.end())
					ui.trap_pool.push_back(code);
				else
					ui.trap_pool.erase(it);
				RefreshTrapPicker();
			}
			return true;
		}
		default:
			return false;
		}
	}
	default:
		return false;
	}
}

bool RkaUI::OnDuelEvent(const irr::SEvent& event) {
	if(event.EventType != irr::EET_GUI_EVENT || !ui.btnSaveState)
		return false;
	const int id = event.GUIEvent.Caller->getID();
	const auto type = event.GUIEvent.EventType;
	if(HandleStateEvent(id, type))
		return true;
	if(type != irr::gui::EGET_BUTTON_CLICKED)
		return false;
	switch(id) {
	case BUTTON_RKA_SAVE_STATE: {
		std::wstring name;
		if(SingleMode::SaveState(&name))
			Toast(epro::format(L"State saved\n{}", name));
		else
			Toast(L"You can save when the game is waiting for you to act.");
		return true;
	}
	case BUTTON_RKA_LOAD_STATE:
		OpenStates();
		return true;
	case BUTTON_REPLAY_TAKEOVER:
		if(!ReplayMode::TakeOver())
			Toast(L"This replay was saved without the data needed to take over.");
		return true;
	default:
		return false;
	}
}

}
