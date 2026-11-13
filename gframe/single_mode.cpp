#include "single_mode.h"
#include "game_config.h"
#include "duelclient.h"
#include "game.h"
#include "core_utils.h"
#include "sound_manager.h"
#include "fmt.h"
#include "localtime.h"
#include "rka_ui.h"
#include "deck_optimizer.h"
#include "client_card.h"
#include <algorithm>
#include <functional>
#include "CGUIFileSelectListBox/CGUIFileSelectListBox.h"
#include <IrrlichtDevice.h>
#include <IGUIWindow.h>
#include <IGUIStaticText.h>
#include <IGUIEditBox.h>
#include <IGUIButton.h>
#include <IGUIContextMenu.h>
#include <IGUITabControl.h>

namespace ygo {

DuelPtr SingleMode::pduel = nullptr;
bool SingleMode::is_closing = false;
bool SingleMode::is_continuing = false;
bool SingleMode::is_restarting = false;
Replay SingleMode::last_replay;
Replay SingleMode::new_replay;
ReplayStream SingleMode::replay_stream;
Signal SingleMode::singleSignal;
epro::thread SingleMode::single_mode_thread;

rka::HandtrapAI SingleMode::ai;
std::shared_ptr<Replay> SingleMode::pending_resume;
std::shared_ptr<Replay> SingleMode::active_resume;
size_t SingleMode::resume_limit = 0;
size_t SingleMode::resume_fed = 0;
int SingleMode::resume_turn = 0;
int SingleMode::turns_seen = 0;
bool SingleMode::catching_up = false;
bool SingleMode::waiting_for_player = false;
bool SingleMode::ai_answered_last = false;
size_t SingleMode::size_before_ai_answer = 0;
CoreUtils::Packet SingleMode::last_selection;
std::string SingleMode::state_deck_name;

namespace {
constexpr epro::stringview hand_test_script = "hand-test-mode";

// the route being played: opening hand and the board at the end of the first turn
std::vector<uint32_t> line_hand;
std::vector<uint32_t> line_board;
bool line_board_taken = false;

std::vector<uint32_t> PlayerCards(bool hand) {
	std::vector<uint32_t> codes;
	std::lock_guard<epro::mutex> lock(mainGame->gMutex);
	auto collect = [&codes](const std::vector<ClientCard*>& cards) {
		for(const auto* card : cards) {
			if(card && card->code)
				codes.push_back(rka::CanonicalCode(card->code));
		}
	};
	if(hand)
		collect(mainGame->dField.hand[0]);
	else {
		collect(mainGame->dField.mzone[0]);
		collect(mainGame->dField.szone[0]);
	}
	return codes;
}

// The hand test stores its extra settings after the script name, so a
// recording carries everything needed to replay or continue it:
// "hand-test-mode;opp=<opponent hand size>;ai=<mode>;deck=<deck name>"
struct HandTestExtras {
	bool has_opponent_hand{ false };
	uint32_t opponent_hand{ 0 };
	rka::HandtrapAI::Mode ai{ rka::HandtrapAI::OFF };
	std::string deck;
};

std::string EncodeHandTestExtras(const HandTestExtras& extras) {
	std::string name{ hand_test_script };
	if(extras.has_opponent_hand)
		name += epro::format(";opp={}", extras.opponent_hand);
	if(extras.ai != rka::HandtrapAI::OFF)
		name += epro::format(";ai={}", static_cast<int>(extras.ai));
	if(!extras.deck.empty())
		name += epro::format(";deck={}", extras.deck);
	return name;
}

int ParseSmallNumber(epro::stringview text) {
	int value = 0;
	for(auto c : text) {
		if(c < '0' || c > '9' || value > 1000)
			break;
		value = value * 10 + (c - '0');
	}
	return value;
}

HandTestExtras DecodeHandTestExtras(epro::stringview name) {
	HandTestExtras extras;
	auto pos = name.find(';');
	while(pos != epro::stringview::npos) {
		auto field = name.substr(pos + 1);
		if(field.substr(0, 5) == "deck=") {
			// always last: a deck name may itself contain ';'
			extras.deck.assign(field.data() + 5, field.size() - 5);
			break;
		}
		const auto next = field.find(';');
		const auto value = field.substr(0, next);
		if(value.substr(0, 4) == "opp=") {
			extras.has_opponent_hand = true;
			extras.opponent_hand = static_cast<uint32_t>(ParseSmallNumber(value.substr(4)));
		} else if(value.substr(0, 3) == "ai=") {
			const auto mode = ParseSmallNumber(value.substr(3));
			if(mode == rka::HandtrapAI::EASY || mode == rka::HandtrapAI::SMART)
				extras.ai = static_cast<rka::HandtrapAI::Mode>(mode);
		}
		pos = next == epro::stringview::npos ? next : pos + 1 + next;
	}
	return extras;
}
}

uint32_t GetHandTestOpponentHand(epro::stringview script_name, uint32_t default_hand) {
	const auto extras = DecodeHandTestExtras(script_name);
	return extras.has_opponent_hand ? extras.opponent_hand : default_hand;
}

bool SingleMode::StartPlay(DuelOptions&& duelOptions) {
	if(mainGame->dInfo.isSingleMode)
		return false;
	if(single_mode_thread.joinable())
		single_mode_thread.join();
	single_mode_thread = epro::thread(SinglePlayThread, std::move(duelOptions));
	return true;
}
void SingleMode::StopPlay(bool is_exiting) {
	is_closing = is_exiting;
	is_continuing = false;
	is_restarting = false;
	mainGame->actionSignal.Set();
	if(is_closing) {
		singleSignal.SetNoWait(true);
		if(single_mode_thread.joinable())
			single_mode_thread.join();
	} else
		singleSignal.Set();
}
void SingleMode::Restart() {
	StopPlay();
	is_restarting = true;
}
epro::path_string SingleMode::GetStateFolder() {
	return epro::format(EPRO_TEXT("{}/"), Replay::GetReplayFilePath(EPRO_TEXT("savestates")));
}
bool SingleMode::CanSaveState() {
	return mainGame->dInfo.isSingleMode && pduel && waiting_for_player && !catching_up;
}
bool SingleMode::SaveState(std::wstring* saved_name) {
	if(!CanSaveState())
		return false;
	// the duel thread is parked waiting for the player, so the recording is stable
	const auto buffer = last_replay.GetUncompressedBuffer();
	if(buffer.size() <= sizeof(ReplayHeader))
		return false;
	Utils::MakeDirectory(GetStateFolder());
	const auto deck = BufferIO::DecodeUTF8(state_deck_name);
	auto label = epro::format(L"{:%Y-%m-%d %H-%M-%S} turn {}", epro::localtime(std::time(nullptr)), mainGame->dInfo.turn);
	if(!deck.empty())
		label += epro::format(L" {}", deck);
	// keep the label usable as a file name on every platform
	for(auto& c : label) {
		if(c == L'/' || c == L'\\' || c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|')
			c = L'_';
	}
	const auto path = epro::format(EPRO_TEXT("{}{}.yrp"), GetStateFolder(), Utils::ToPathString(label));
	auto* file = fileopen(path.data(), "wb");
	if(file == nullptr)
		return false;
	const auto written = fwrite(buffer.data(), 1, buffer.size(), file);
	fclose(file);
	if(written != buffer.size()) {
		Utils::FileDelete(path);
		return false;
	}
	if(saved_name)
		*saved_name = label;
	return true;
}
std::vector<epro::path_string> SingleMode::ListStates() {
	auto files = Utils::FindFiles(GetStateFolder(), { EPRO_TEXT("yrp") });
	// names start with the date, so this puts the newest first
	std::sort(files.begin(), files.end(), std::greater<>());
	return files;
}
bool SingleMode::DeleteState(const epro::path_string& file) {
	return Utils::FileDelete(epro::format(EPRO_TEXT("{}{}"), GetStateFolder(), file));
}
bool SingleMode::LoadState(const epro::path_string& file, DuelOptions::ReturnTo return_to) {
	auto state = std::make_shared<Replay>();
	if(!state->OpenReplay(epro::format(EPRO_TEXT("{}{}"), GetStateFolder(), file)))
		return false;
	if(!state->IsOldReplayMode() || !state->CanBePlayedInOldMode())
		return false;
	if(mainGame->dInfo.isSingleMode) {
		// swap the running duel for the saved one
		pending_resume = std::move(state);
		Restart();
		return true;
	}
	DuelOptions options;
	options.returnTo = return_to;
	options.resume = std::move(state);
	singleSignal.SetNoWait(false);
	return StartPlay(std::move(options));
}
void SingleMode::SetResponse(void* resp, size_t len) {
	if(!pduel)
		return;
	last_replay.Write<uint8_t>(static_cast<uint8_t>(len), false);
	last_replay.WriteData(resp, len);
	pduel->DuelSetResponse(resp, static_cast<uint32_t>(len));
}
void SingleMode::EndCatchUp() {
	if(!catching_up)
		return;
	catching_up = false;
	active_resume = nullptr;
	std::lock_guard<epro::mutex> lock(mainGame->gMutex);
	mainGame->dInfo.isCatchingUp = false;
	mainGame->dField.RefreshAllCards();
}
bool SingleMode::FeedRecordedResponse() {
	if(!catching_up || !active_resume || resume_fed >= resume_limit)
		return false;
	ReplayResponse* recorded = nullptr;
	if(!active_resume->GetNextResponse(recorded))
		return false;
	++resume_fed;
	SetResponse(recorded->response.data(), recorded->length);
	return true;
}
int SingleMode::SinglePlayThread(DuelOptions&& duelOptions) {
	Utils::SetThreadName("SinglePlay");
	uint64_t opt = duelOptions.duelFlags;
	std::string script_name = "";
	std::shared_ptr<Replay> resume;
	auto WriteName = [](Replay& replay, const std::wstring& name) {
		uint16_t buffer[20];
		BufferIO::EncodeUTF16(name.data(), buffer, 20);
		replay.WriteData(buffer, 40, false);
	};
	auto InitReplay = [&]() {
		if(resume) {
			// same players, rules and decks as the recording being continued
			last_replay.WriteData(resume->replay_data.data(), resume->responses_offset, false);
			last_replay.Flush();
			if(resume->pheader.base.flag & REPLAY_SINGLE_MODE) {
				WriteName(new_replay, mainGame->dInfo.selfnames[0]);
				WriteName(new_replay, mainGame->dInfo.opponames[0]);
			} else {
				new_replay.Write<uint32_t>(static_cast<uint32_t>(mainGame->dInfo.selfnames.size()), false);
				for(const auto& name : mainGame->dInfo.selfnames)
					WriteName(new_replay, name);
				new_replay.Write<uint32_t>(static_cast<uint32_t>(mainGame->dInfo.opponames.size()), false);
				for(const auto& name : mainGame->dInfo.opponames)
					WriteName(new_replay, name);
			}
			new_replay.Write<uint64_t>(opt);
			return;
		}
		WriteName(last_replay, mainGame->dInfo.selfnames[0]);
		WriteName(new_replay, mainGame->dInfo.selfnames[0]);
		WriteName(last_replay, mainGame->dInfo.opponames[0]);
		WriteName(new_replay, mainGame->dInfo.opponames[0]);
		last_replay.Write<uint32_t>(duelOptions.startingLP, false);
		last_replay.Write<uint32_t>(duelOptions.startingDrawCount, false);
		last_replay.Write<uint32_t>(duelOptions.drawCountPerTurn, false);
		last_replay.Write<uint64_t>(opt, false);
		last_replay.Write<uint16_t>((uint16_t)script_name.size(), false);
		last_replay.WriteData(script_name.data(), script_name.size(), false);
		last_replay.Flush();
		new_replay.Write<uint64_t>(opt);
	};
	mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 45));
	is_continuing = false;
	is_restarting = false;
	auto rnd = Utils::GetRandomNumberGenerator();
	// where to go back to when the duel is closed: decided once, a loaded state does not change it
	const bool starts_as_hand_test = duelOptions.resume ? !!(duelOptions.resume->pheader.base.flag & REPLAY_HAND_TEST) : (duelOptions.scriptName == hand_test_script);
	const bool to_replay_list = duelOptions.returnTo == DuelOptions::RETURN_REPLAY_LIST;
	const bool to_deck_edit = duelOptions.returnTo == DuelOptions::RETURN_DECK_EDIT || (duelOptions.returnTo == DuelOptions::RETURN_AUTO && starts_as_hand_test);
restart:
	// a state picked from the list wins over whatever this session was started with
	resume = std::exchange(pending_resume, nullptr);
	resume_limit = static_cast<size_t>(-1);
	resume_turn = 0;
	if(!resume && duelOptions.resume) {
		resume = duelOptions.resume;
		resume_limit = duelOptions.resumeResponses;
		resume_turn = duelOptions.resumeTurn;
	}
	if(resume) {
		resume->Rewind();
		opt = resume->params.duel_flags;
	}
	line_hand.clear();
	line_board.clear();
	line_board_taken = false;
	resume_fed = 0;
	turns_seen = 0;
	catching_up = false;
	waiting_for_player = false;
	ai_answered_last = false;
	const uint32_t replay_flags = resume ? resume->pheader.base.flag : 0;
	// a recording of a normal duel: both sides are loaded from the recording
	const bool recorded_duel = resume && !(replay_flags & REPLAY_SINGLE_MODE);
	mainGame->dInfo.isSingleMode = true;
	bool hand_test = mainGame->dInfo.isHandTest = resume ? !!(replay_flags & REPLAY_HAND_TEST) : (duelOptions.scriptName == hand_test_script);
	HandTestExtras extras;
	if(resume) {
		if(hand_test)
			extras = DecodeHandTestExtras(resume->scriptname);
	} else if(hand_test) {
		opt |= DUEL_ATTACK_FIRST_TURN;
		extras.ai = duelOptions.aiMode;
		extras.deck = duelOptions.deckName;
		if(extras.ai != rka::HandtrapAI::OFF) {
			extras.has_opponent_hand = true;
			extras.opponent_hand = static_cast<uint32_t>(duelOptions.opponentHand.size());
		}
	}
	state_deck_name = extras.deck;
	OCG_Player team = { duelOptions.startingLP, duelOptions.startingDrawCount, duelOptions.drawCountPerTurn };
	if(resume)
		team = { resume->params.start_lp, resume->params.start_hand, resume->params.draw_count };
	OCG_Player opposing_team = team;
	if(extras.has_opponent_hand)
		opposing_team.startingDrawCount = extras.opponent_hand;
	auto seed = Utils::GetRandomNumberGeneratorSeed();
	if(resume) {
		for(int i = 0; i < 4; ++i)
			seed[i] = resume->pheader.seed[i];
	}
	pduel = mainGame->SetupDuel({ { seed[0], seed[1], seed[2], seed[3] }, opt, team, opposing_team });
	mainGame->dInfo.duel_params = opt;
	mainGame->dInfo.duel_field = mainGame->GetMasterRule(mainGame->dInfo.duel_params);
	matManager.SetActiveVertices(mainGame->dInfo.HasFieldFlag(DUEL_3_COLUMNS_FIELD),
								 !mainGame->dInfo.HasFieldFlag(DUEL_SEPARATE_PZONE));
	mainGame->dInfo.compat_mode = false;
	mainGame->dInfo.legacy_race_size = false;
	mainGame->dInfo.startlp = mainGame->dInfo.lp[0] = mainGame->dInfo.lp[1] = team.startingLP;
	mainGame->dInfo.strLP[0] = mainGame->dInfo.strLP[1] = epro::to_wstring(mainGame->dInfo.lp[0]);
	mainGame->dInfo.selfnames = { mainGame->ebNickName->getText() };
	mainGame->dInfo.opponames = { L"" };
	mainGame->dInfo.isRelay = false;
	mainGame->dInfo.team1 = mainGame->dInfo.team2 = 1;
	mainGame->dInfo.current_player[0] = mainGame->dInfo.current_player[1] = 0;
	if(resume) {
		const auto& names = resume->GetPlayerNames();
		const auto home = static_cast<size_t>(resume->GetPlayersCount(0));
		if(home > 0 && home < names.size()) {
			mainGame->dInfo.selfnames.assign(names.begin(), names.begin() + home);
			mainGame->dInfo.opponames.assign(names.begin() + home, names.end());
		}
		if(recorded_duel) {
			mainGame->dInfo.isRelay = !!(opt & DUEL_RELAY);
			mainGame->dInfo.team1 = resume->GetPlayersCount(0);
			mainGame->dInfo.team2 = resume->GetPlayersCount(1);
			if(!mainGame->dInfo.isRelay)
				mainGame->dInfo.current_player[1] = mainGame->dInfo.team2 - 1;
		}
	} else if(extras.ai != rka::HandtrapAI::OFF)
		mainGame->dInfo.opponames = { extras.ai == rka::HandtrapAI::SMART ? L"Hand traps (smart)" : L"Hand traps (easy)" };
	mainGame->dInfo.player_type = 0;
	mainGame->dInfo.turn = 0;
	bool loaded = true;
	bool saveReplay = !hand_test || gGameConfig->saveHandTest;
	{
		// the duel is always recorded in memory: save states are cut from that recording
		auto replay_header = ExtendedReplayHeader::CreateDefaultHeader(REPLAY_YRP1, static_cast<uint32_t>(time(nullptr)));
		replay_header.SetSeed(seed);
		if(resume) {
			replay_header.base.flag = (replay_flags & ~REPLAY_COMPRESSED) | REPLAY_EXTENDED_HEADER;
			replay_header.base.version = resume->pheader.base.version;
		} else {
			replay_header.base.flag |= REPLAY_SINGLE_MODE;
			if(hand_test)
				replay_header.base.flag |= REPLAY_HAND_TEST;
		}
		last_replay.BeginRecord(saveReplay, Replay::GetReplayFilePath(EPRO_TEXT("_LastReplay.yrp")));
		last_replay.WriteHeader(replay_header);
		if(saveReplay) {
			//records the replay with the new system
			new_replay.BeginRecord();
			replay_header.base.id = REPLAY_YRPX;
			new_replay.WriteHeader(replay_header);
		}
		replay_stream.clear();
	}
	std::vector<uint32_t> player_deck_codes;
	if(resume && (recorded_duel || hand_test)) {
		script_name = resume->scriptname;
		InitReplay();
		OCG_NewCardInfo card_info = { 0, 0, 0, 0, 0, 0, POS_FACEDOWN_DEFENSE };
		for(auto card : resume->GetRuleCards()) {
			card_info.code = card;
			pduel->DuelNewCard(&card_info);
		}
		const auto& decks = resume->GetPlayerDecks();
		const size_t home = static_cast<size_t>(resume->GetPlayersCount(0));
		const size_t away = static_cast<size_t>(resume->GetPlayersCount(1));
		loaded = decks.size() >= home + away;
		for(size_t i = 0; loaded && i < home + away; ++i) {
			card_info.team = card_info.con = i < home ? 0 : 1;
			card_info.duelist = static_cast<uint8_t>(i < home ? i : i - home);
			card_info.loc = LOCATION_DECK;
			for(auto card : decks[i].main_deck) {
				card_info.code = card;
				pduel->DuelNewCard(&card_info);
			}
			card_info.loc = LOCATION_EXTRA;
			for(auto card : decks[i].extra_deck) {
				card_info.code = card;
				pduel->DuelNewCard(&card_info);
			}
		}
		if(loaded) {
			player_deck_codes = decks[0].main_deck;
			if(hand_test) {
				const char cmd[] = "Debug.ReloadFieldEnd()";
				loaded = pduel->LoadScript(cmd, sizeof(cmd) - 1, " ");
			}
		}
	} else if(resume) {
		// recording of a puzzle
		script_name = resume->scriptname;
		loaded = mainGame->LoadScript(pduel.get(), script_name);
		InitReplay();
	} else if(hand_test) {
		script_name = EncodeHandTestExtras(extras);
		InitReplay();
		Deck playerdeck(mainGame->deckBuilder.GetCurrentDeck());
		if ((duelOptions.duelFlags & DUEL_PSEUDO_SHUFFLE) == 0)
			std::shuffle(playerdeck.main.begin(), playerdeck.main.end(), rnd);
		{
			// cards the player asked to open with go on top of the deck
			size_t placed = 0;
			for(auto wanted : duelOptions.startingHand) {
				auto it = std::find_if(playerdeck.main.begin() + placed, playerdeck.main.end(),
									   [wanted](const CardDataC* card) { return card->code == wanted; });
				if(it == playerdeck.main.end())
					continue;
				std::rotate(playerdeck.main.begin() + placed, it, it + 1);
				++placed;
			}
		}
		for(const auto* card : playerdeck.main)
			player_deck_codes.push_back(card->code);
		auto LoadCards = [&](uint8_t team, uint8_t location, const std::vector<uint32_t>& codes) {
			OCG_NewCardInfo card_info = { team, 0, 0, team, 0, 0, POS_FACEDOWN_DEFENSE };
			card_info.loc = location;
			last_replay.Write<uint32_t>(static_cast<uint32_t>(codes.size()), false);
			for(auto it = codes.rbegin(); it != codes.rend(); ++it) {
				card_info.code = *it;
				pduel->DuelNewCard(&card_info);
				last_replay.Write<uint32_t>(*it, false);
			}
		};
		std::vector<uint32_t> extra_codes;
		for(const auto* card : playerdeck.extra)
			extra_codes.push_back(card->code);
		LoadCards(0, LOCATION_DECK, player_deck_codes);
		LoadCards(0, LOCATION_EXTRA, extra_codes);
		if(extras.ai != rka::HandtrapAI::OFF) {
			// the built-in opponent's whole deck is the hand traps it starts with
			LoadCards(1, LOCATION_DECK, duelOptions.opponentHand);
			LoadCards(1, LOCATION_EXTRA, {});
		} else if (duelOptions.handTestNoOpponent) {
			last_replay.Write<uint32_t>(0, false);
			last_replay.Write<uint32_t>(0, false);
		} else {
			LoadCards(1, LOCATION_DECK, player_deck_codes);
			LoadCards(1, LOCATION_EXTRA, extra_codes);
		}
		last_replay.Flush();
		const char cmd[] = "Debug.ReloadFieldEnd()";
		loaded = pduel->LoadScript(cmd, sizeof(cmd) - 1, " ");
	} else {
		if(open_file) {
			script_name = Utils::ToUTF8IfNeeded(open_file_name);
			if(!mainGame->LoadScript(pduel.get(), script_name)) {
				script_name = epro::format("./puzzles/{}", script_name);
				loaded = mainGame->LoadScript(pduel.get(), script_name);
			}
		} else {
			script_name = duelOptions.scriptName;
			loaded = mainGame->LoadScript(pduel.get(), script_name);
		}
		InitReplay();
	}
	{
		// what the smart opponent thinks each of the player's cards is worth stopping
		bool any_starter = false;
		for(auto code : player_deck_codes) {
			if(rka::RoleStore::Get().Lookup(extras.deck, rka::CanonicalCode(code)) == rka::ROLE_STARTER) {
				any_starter = true;
				break;
			}
		}
		ai.Reset(hand_test ? extras.ai : rka::HandtrapAI::OFF, [deck = extras.deck, any_starter](uint32_t code) {
			switch(rka::RoleStore::Get().Lookup(deck, code)) {
			case rka::ROLE_STARTER: return 3;
			case rka::ROLE_ENGINE_REQ: return 2;
			case rka::ROLE_EXTENDER: return 1;
			// until the player has tagged starters, anything untagged might be one
			case rka::ROLE_NONE: return any_starter ? 1 : 2;
			default: return 0;
			}
		});
	}
	if(!loaded) {
		pduel = nullptr;
		mainGame->dInfo.isSingleMode = false;
		mainGame->dInfo.isHandTest = false;
		open_file = false;
		last_replay.EndRecord();
		new_replay.EndRecord();
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		if(is_restarting) {
			mainGame->dInfo.isInDuel = false;
			mainGame->dInfo.isStarted = false;
			mainGame->dInfo.isSingleMode = false;
			mainGame->dInfo.isHandTest = false;
			if(!to_deck_edit) {
				mainGame->closeDuelWindow = true;
				mainGame->closeDoneSignal.Wait(lock);
			}
			mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 80));
			if(!to_deck_edit) {
				mainGame->ShowElement(to_replay_list ? mainGame->wReplay : mainGame->wSinglePlay);
				mainGame->stTip->setVisible(false);
			}
			mainGame->SetMessageWindow();
			mainGame->device->setEventReceiver(&mainGame->menuHandler);
			if(to_deck_edit) {
				mainGame->btnChainIgnore->setVisible(false);
				mainGame->btnChainAlways->setVisible(false);
				mainGame->btnChainWhenAvail->setVisible(false);
				mainGame->btnCancelOrFinish->setVisible(false);
				mainGame->btnShuffle->setVisible(false);
				mainGame->wChat->setVisible(false);
				mainGame->btnRestartSingle->setVisible(false);
				RkaUI::ShowDuelButtons(false);
				mainGame->wPhase->setVisible(false);
				mainGame->deckBuilder.Initialize(false);
			}
		} else {
			mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 80));
			if(to_replay_list) {
				// a takeover that could not start: back to the replay list
				mainGame->ShowElement(mainGame->wReplay);
				mainGame->device->setEventReceiver(&mainGame->menuHandler);
			}
		}
		is_restarting = false;
		return 0;
	}
	mainGame->gMutex.lock();
	if(!to_deck_edit && !is_restarting && !to_replay_list) {
		mainGame->HideElement(mainGame->wSinglePlay);
		mainGame->ClearCardInfo();
	}
	is_restarting = false;
	mainGame->mTopMenu->setVisible(false);
	mainGame->wCardImg->setVisible(true);
	mainGame->wInfos->setVisible(true);
	mainGame->btnLeaveGame->setVisible(true);
	mainGame->btnLeaveGame->setText(gDataManager->GetSysString(1210).data());
	mainGame->btnRestartSingle->setVisible(true);
	RkaUI::ShowDuelButtons(true);
	mainGame->wPhase->setVisible(true);
	mainGame->dField.Clear();
	mainGame->dInfo.isFirst = true;
	mainGame->dInfo.isTeam1 = true;
	mainGame->dInfo.isInDuel = true;
	mainGame->dInfo.isStarted = true;
	mainGame->dInfo.isCatchingUp = false;
	mainGame->dInfo.checkRematch = false;
	mainGame->SetMessageWindow();
	mainGame->device->setEventReceiver(&mainGame->dField);
	if(resume) {
		// replay the recording without animations or pauses, like catching up after a reconnect
		active_resume = resume;
		catching_up = true;
		mainGame->dInfo.isCatchingUp = true;
		if(recorded_duel)
			mainGame->SetPhaseButtons();
	}
	mainGame->gMutex.unlock();
	is_closing = false;
	is_continuing = true;
	int engFlag = 0;
	if(recorded_duel) {
		// a normal duel starts with the server announcing both decks
		uint8_t startbuf[32];
		auto* pbuf = startbuf;
		BufferIO::Write<uint8_t>(pbuf, 0);
		BufferIO::Write<uint32_t>(pbuf, team.startingLP);
		BufferIO::Write<uint32_t>(pbuf, team.startingLP);
		BufferIO::Write<uint16_t>(pbuf, static_cast<uint16_t>(pduel->DuelQueryCount(0, LOCATION_DECK)));
		BufferIO::Write<uint16_t>(pbuf, static_cast<uint16_t>(pduel->DuelQueryCount(0, LOCATION_EXTRA)));
		BufferIO::Write<uint16_t>(pbuf, static_cast<uint16_t>(pduel->DuelQueryCount(1, LOCATION_DECK)));
		BufferIO::Write<uint16_t>(pbuf, static_cast<uint16_t>(pduel->DuelQueryCount(1, LOCATION_EXTRA)));
		CoreUtils::Packet start{ MSG_START, startbuf, 17 };
		is_continuing = SinglePlayAnalyze(start) && is_continuing;
	}
	for(auto& message : CoreUtils::ParseMessages(pduel.get()))
		is_continuing = SinglePlayAnalyze(message) && is_continuing;
	if(is_continuing) {
		pduel->StartDuel();
		if(recorded_duel) {
			SinglePlayRefresh(0, LOCATION_DECK, 0x2181fff);
			SinglePlayRefresh(1, LOCATION_DECK, 0x2181fff);
			SinglePlayRefresh(0, LOCATION_EXTRA, 0x2181fff);
			SinglePlayRefresh(1, LOCATION_EXTRA, 0x2181fff);
		}
		do {
			engFlag = pduel->DuelProcess();
			for(auto& message : CoreUtils::ParseMessages(pduel.get())) {
				if(message.message == MSG_WIN && hand_test)
					continue;
				is_continuing = SinglePlayAnalyze(message) && is_continuing;
			}
		} while(is_continuing && engFlag && mainGame->dInfo.curMsg != MSG_WIN);
	}
	EndCatchUp();
	waiting_for_player = false;
	pduel = nullptr;
	if(saveReplay && !is_restarting) {
		last_replay.EndRecord(0x1000);
		auto oldbuffer = last_replay.GetSerializedBuffer();
		CoreUtils::Packet tmp{};
		tmp.message = OLD_REPLAY_MODE;
		tmp.buffer.swap(oldbuffer);
		new_replay.WritePacket(tmp);
		new_replay.EndRecord();
	}
	if(is_closing) {
		open_file = false;
		is_restarting = false;
		mainGame->gMutex.lock();
		mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 80));
		mainGame->gMutex.unlock();
		return 0;
	}
	gSoundManager->StopSounds();
	bool was_restarting = is_restarting;
	if(saveReplay && !was_restarting) {
		auto now = std::time(nullptr);
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->PopupSaveWindow(gDataManager->GetSysString(1340), epro::format(L"{:%Y-%m-%d %H-%M-%S}", epro::localtime(now)), gDataManager->GetSysString(1342));
		mainGame->replaySignal.Wait(lock);
		if(mainGame->saveReplay) {
			new_replay.SaveReplay(Utils::ToPathString(mainGame->ebFileSaveName->getText()));
			// a saved hand test is a route the player wants remembered
			if(hand_test && !line_hand.empty()) {
				lock.unlock();
				if(!line_board_taken)
					line_board = PlayerCards(false);
				rka::LineStore::Get().Add(state_deck_name, line_hand, line_board, BufferIO::EncodeUTF8(mainGame->ebFileSaveName->getText()));
				lock.lock();
			}
		}
	}
	new_replay.Reset();
	last_replay.Reset();
	mainGame->gMutex.lock();
	mainGame->dField.Clear();
	mainGame->gMutex.unlock();
	if(!is_closing) {
		if(was_restarting || to_deck_edit) {
			std::lock_guard<epro::mutex> lock(mainGame->gMutex);
			for(auto wit = mainGame->fadingList.begin(); wit != mainGame->fadingList.end(); ++wit) {
				if(wit->isFadein)
					wit->autoFadeoutFrame = 1;
			}
			mainGame->wACMessage->setVisible(false);
			mainGame->wANAttribute->setVisible(false);
			mainGame->wANCard->setVisible(false);
			mainGame->wANNumber->setVisible(false);
			mainGame->wANRace->setVisible(false);
			mainGame->wCardSelect->setVisible(false);
			mainGame->wCardDisplay->setVisible(false);
			mainGame->wCmdMenu->setVisible(false);
			mainGame->wMessage->setVisible(false);
			mainGame->wOptions->setVisible(false);
			mainGame->wPosSelect->setVisible(false);
			mainGame->wQuery->setVisible(false);
			mainGame->stHintMsg->setVisible(false);
			RkaUI::HideDuelWindows();
			if(was_restarting)
				goto restart;
		}
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->dInfo.isInDuel = false;
		mainGame->dInfo.isStarted = false;
		mainGame->dInfo.isSingleMode = false;
		mainGame->dInfo.isHandTest = false;
		if(!to_deck_edit) {
			mainGame->closeDuelWindow = true;
			mainGame->closeDoneSignal.Wait(lock);
		}
		mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 80));
		if(!to_deck_edit) {
			mainGame->ShowElement(to_replay_list ? mainGame->wReplay : mainGame->wSinglePlay);
			mainGame->stTip->setVisible(false);
		}
		mainGame->SetMessageWindow();
		mainGame->device->setEventReceiver(&mainGame->menuHandler);
		if(to_deck_edit) {
			mainGame->btnChainIgnore->setVisible(false);
			mainGame->btnChainAlways->setVisible(false);
			mainGame->btnChainWhenAvail->setVisible(false);
			mainGame->btnCancelOrFinish->setVisible(false);
			mainGame->btnShuffle->setVisible(false);
			mainGame->wChat->setVisible(false);
			mainGame->btnRestartSingle->setVisible(false);
			RkaUI::ShowDuelButtons(false);
			mainGame->wPhase->setVisible(false);
			mainGame->deckBuilder.Initialize(false);
		}
	}
	open_file = false;
	return 0;
}

bool SingleMode::SinglePlayAnalyze(CoreUtils::Packet& packet) {
	auto Analyze = [&packet]()->bool {
		DuelClient::answered = false;
		return DuelClient::ClientAnalyze(packet);
	};
	// shows a prompt to the player and waits for the answer
	auto AskPlayer = [&Analyze]() {
		ai_answered_last = false;
		waiting_for_player = true;
		if(!Analyze())
			singleSignal.Wait();
		waiting_for_player = false;
	};
	replay_stream.clear();
	if(is_closing || !is_continuing)
		return false;
	mainGame->dInfo.curMsg = packet.message;
	bool record = true;
	bool record_last = false;
	ai.Observe(packet);
	switch(mainGame->dInfo.curMsg) {
		case MSG_RETRY:	{
			EndCatchUp();
			if(ai_answered_last) {
				// the built-in opponent got it wrong: take its answer back and let the player choose
				ai_answered_last = false;
				if(last_replay.replay_data.size() >= size_before_ai_answer)
					last_replay.replay_data.resize(size_before_ai_answer);
				mainGame->dInfo.curMsg = last_selection.message;
				waiting_for_player = true;
				DuelClient::answered = false;
				if(!DuelClient::ClientAnalyze(last_selection))
					singleSignal.Wait();
				waiting_for_player = false;
				return is_continuing;
			}
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->stMessage->setText(gDataManager->GetSysString(1434).data());
			mainGame->PopupElement(mainGame->wMessage);
			mainGame->actionSignal.Wait(lock);
			return false;
		}
		case MSG_NEW_TURN: {
			++turns_seen;
			if(turns_seen == 2 && !line_board_taken) {
				line_board = PlayerCards(false);
				line_board_taken = true;
			}
			// taking over "from this turn": everything after this point is the player's
			if(catching_up && resume_turn > 0 && turns_seen >= resume_turn)
				resume_limit = resume_fed;
			Analyze();
			break;
		}
		case MSG_HINT: {
			const auto* pbuf = packet.data();
			int type = BufferIO::Read<uint8_t>(pbuf);
			auto player = BufferIO::Read<uint8_t>(pbuf);
			/*uint64_t data = BufferIO::Read<uint64_t>(pbuf);*/
			bool analyze = false;
			switch (type) {
			case 1:
			case 2:
			case 3:
			case 5: {
				analyze = player == 0;
				break;
			}
			case 4:
			case 6:
			case 7:
			case 8:
			case 9:
			case 11: {
				analyze = player != 0;
				break;
			}
			case 10:
			case 200:
			case 201:
			case 202:
			case 203: {
				analyze = true;
				break;

			}
			}
			if(analyze)
				Analyze();
			if(type > 0 && type < 6 && type != 4)
				record = false;
			break;
		}
		case MSG_AI_NAME:
		case MSG_SHOW_HINT: {
			auto* pbuf = packet.data();
			auto len = BufferIO::Read<uint16_t>(pbuf);
			if((len + 1u) != packet.buff_size() - (sizeof(uint16_t)))
				break;
			pbuf[len] = 0;
			if(packet.message == MSG_AI_NAME) {
				mainGame->dInfo.opponames[0] = BufferIO::DecodeUTF8({ reinterpret_cast<char*>(pbuf), len });
			} else if(!catching_up) {
				std::unique_lock<epro::mutex> lock(mainGame->gMutex);
				mainGame->stMessage->setText(BufferIO::DecodeUTF8({ reinterpret_cast<char*>(pbuf), len }).data());
				mainGame->PopupElement(mainGame->wMessage);
				mainGame->actionSignal.Wait(lock);
			}
			break;
		}
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
		case MSG_ANNOUNCE_NUMBER: {
			record = false;
			const bool command = mainGame->dInfo.curMsg == MSG_SELECT_BATTLECMD || mainGame->dInfo.curMsg == MSG_SELECT_IDLECMD;
			if(catching_up) {
				// still inside the recording: answer exactly as it was answered back then
				if(FeedRecordedResponse())
					break;
				EndCatchUp();
			}
			if(command)
				SinglePlayRefresh();
			if(command && turns_seen == 1 && line_hand.empty())
				line_hand = PlayerCards(true);
			if(command) {
			} else if(mainGame->dInfo.curMsg == MSG_SELECT_CHAIN) {
				SinglePlayRefresh(0, LOCATION_MZONE);
				SinglePlayRefresh(1, LOCATION_MZONE);
				SinglePlayRefresh(0, LOCATION_SZONE);
				SinglePlayRefresh(1, LOCATION_SZONE);
				record_last = true;
			}
			std::vector<uint8_t> answer;
			if(ai.Respond(packet, answer)) {
				ai_answered_last = true;
				last_selection = packet;
				size_before_ai_answer = last_replay.replay_data.size();
				SetResponse(answer.data(), answer.size());
				break;
			}
			AskPlayer();
			break;
		}
		default: {
			Analyze();
			break;
		}
	}
	auto* pbuf = packet.data();
	switch(mainGame->dInfo.curMsg) {
		case MSG_SHUFFLE_DECK: {
			auto player = BufferIO::Read<uint8_t>(pbuf);
			SinglePlayRefresh(player, LOCATION_DECK, 0x2181fff);
			break;
		}
		case MSG_SWAP_GRAVE_DECK: {
			auto player = BufferIO::Read<uint8_t>(pbuf);
			SinglePlayRefresh(player, LOCATION_GRAVE, 0x2181fff);
			break;
		}
		case MSG_REVERSE_DECK: {
			SinglePlayRefresh(0, LOCATION_DECK, 0x2181fff);
			SinglePlayRefresh(1, LOCATION_DECK, 0x2181fff);
			break;
		}
		case MSG_MOVE: {
			pbuf += 4;
			auto previous = CoreUtils::ReadLocInfo(pbuf, false);
			auto current = CoreUtils::ReadLocInfo(pbuf,false);
			if(previous.location && !(current.location & 0x80) && (previous.location != current.location || previous.controler != current.controler))
				SinglePlayRefreshSingle(current.controler, current.location, current.sequence);
			break;
		}
		case MSG_TAG_SWAP: {
			auto player = BufferIO::Read<uint8_t>(pbuf);
			SinglePlayRefresh(player, LOCATION_DECK, 0x181fff);
			SinglePlayRefresh(player, LOCATION_EXTRA, 0x181fff);
			break;
		}
		case MSG_NEW_PHASE:
		case MSG_SUMMONED:
		case MSG_SPSUMMONED:
		case MSG_FLIPSUMMONED:
		case MSG_CHAINED:
		case MSG_CHAIN_SOLVED:
		case MSG_DAMAGE_STEP_START:
		case MSG_DAMAGE_STEP_END: {
			SinglePlayRefresh();
			break;
		}
		case MSG_CHAIN_END:	{
			SinglePlayRefresh();
			SinglePlayRefresh(0, LOCATION_DECK);
			SinglePlayRefresh(1, LOCATION_DECK);
			break;
		}
		case MSG_RELOAD_FIELD: {
			SinglePlayReload();
			std::lock_guard<epro::mutex> lock(mainGame->gMutex);
			mainGame->dField.RefreshAllCards();
			break;
		}
	}
	if(record)
		replay_stream.insert(record_last ? replay_stream.end() : replay_stream.begin(), std::move(packet));
	new_replay.WriteStream(replay_stream);
	new_replay.Flush();
	return is_continuing;
}
void SingleMode::SinglePlayRefresh(uint8_t player, uint8_t location, uint32_t flag) {
	std::vector<uint8_t> buffer;
	uint32_t len = 0;
	OCG_QueryInfo info{ flag, player, location };
	auto buff = pduel->DuelQueryLocation(&len, &info);
	if(len == 0)
		return;
	buffer.resize(buffer.size() + len);
	memcpy(buffer.data(), buff, len);
	mainGame->gMutex.lock();
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(player), location, buffer.data());
	mainGame->gMutex.unlock();
	buffer.insert(buffer.begin(), location);
	buffer.insert(buffer.begin(), player);
	replay_stream.emplace_back(MSG_UPDATE_DATA, buffer.data(), buffer.size());
}
void SingleMode::SinglePlayRefreshSingle(uint8_t player, uint8_t location, uint8_t sequence, uint32_t flag) {
	std::vector<uint8_t> buffer;
	uint32_t len = 0;
	OCG_QueryInfo info{ flag, player, location, sequence };
	auto buff = pduel->DuelQuery(&len, &info);
	if(buff == nullptr)
		return;
	buffer.resize(buffer.size() + len);
	memcpy(buffer.data(), buff, len);
	mainGame->gMutex.lock();
	mainGame->dField.UpdateCard(mainGame->LocalPlayer(player), location, sequence, buffer.data());
	mainGame->gMutex.unlock();
	buffer.insert(buffer.begin(), sequence);
	buffer.insert(buffer.begin(), location);
	buffer.insert(buffer.begin(), player);
	replay_stream.emplace_back(MSG_UPDATE_CARD, buffer.data(), buffer.size());
}
void SingleMode::SinglePlayRefresh(uint32_t flag) {
	for(int p = 0; p < 2; p++)
		for(int loc = LOCATION_HAND; loc != LOCATION_GRAVE; loc *= 2)
			SinglePlayRefresh(p, loc, flag);
}
void SingleMode::SinglePlayReload() {
	for(int p = 0; p < 2; p++)
		for(int loc = LOCATION_DECK; loc != LOCATION_OVERLAY; loc *= 2)
			SinglePlayRefresh(p, loc, 0x2ffdfff);
}

}
