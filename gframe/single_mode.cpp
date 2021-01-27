#include "single_mode.h"
#include <fmt/chrono.h>
#include "random_fwd.h"
#include "game_config.h"
#include "duelclient.h"
#include "game.h"
#include "core_utils.h"
#include "sound_manager.h"
#include "CGUIFileSelectListBox/CGUIFileSelectListBox.h"
#include <IrrlichtDevice.h>
#include <IGUIWindow.h>
#include <IGUIStaticText.h>
#include <IGUIEditBox.h>
#include <IGUIButton.h>
#include <IGUIContextMenu.h>
#include <IGUITabControl.h>

namespace ygo {

OCG_Duel SingleMode::pduel = 0;
bool SingleMode::is_closing = false;
bool SingleMode::is_continuing = false;
bool SingleMode::is_restarting = false;
Replay SingleMode::last_replay;
Replay SingleMode::new_replay;
ReplayStream SingleMode::replay_stream;
Signal SingleMode::singleSignal;

bool SingleMode::StartPlay(const DuelOptions& duelOptions) {
	if(mainGame->dInfo.isSingleMode)
		return false;
	std::thread(SinglePlayThread, duelOptions).detach();
	return true;
}
void SingleMode::StopPlay(bool is_exiting) {
	is_closing = is_exiting;
	is_continuing = false;
	is_restarting = false;
	mainGame->actionSignal.Set();
	if(is_closing)
		singleSignal.SetNoWait(true);
	else
		singleSignal.Set();
}
void SingleMode::Restart() {
	StopPlay();
	is_restarting = true;
}
void SingleMode::SetResponse(void* resp, uint32_t len) {
	if(!pduel)
		return;
	last_replay.Write<uint8_t>(len, false);
	last_replay.WriteData(resp, len);
	OCG_DuelSetResponse(pduel, resp, len);
}
int SingleMode::SinglePlayThread(DuelOptions duelOptions) {
	Utils::SetThreadName("SinglePlay");
	uint64_t opt = duelOptions.duelFlags;
	std::string script_name = "";
	auto InitReplay = [&]() {
		uint16_t buffer[20];
		BufferIO::CopyWStr(mainGame->dInfo.selfnames[0].data(), buffer, 20);
		last_replay.WriteData(buffer, 40, false);
		new_replay.WriteData(buffer, 40, false);
		BufferIO::CopyWStr(mainGame->dInfo.opponames[0].data(), buffer, 20);
		last_replay.WriteData(buffer, 40, false);
		new_replay.WriteData(buffer, 40, false);
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
restart:
	uint32_t seed = static_cast<uint32_t>(time(0));;
	DuelClient::rnd.seed(seed);
	mainGame->dInfo.isSingleMode = true;
	OCG_Player team = { duelOptions.startingLP, duelOptions.startingDrawCount, duelOptions.drawCountPerTurn };
	bool hand_test = mainGame->dInfo.isHandTest = open_file && open_file_name == EPRO_TEXT("hand-test-mode");
	if(hand_test)
		opt |= DUEL_ATTACK_FIRST_TURN;
	pduel = mainGame->SetupDuel({ DuelClient::rnd(), opt, team, team });
	mainGame->dInfo.compat_mode = false;
	///////////kdiy///////////
	mainGame->dInfo.startlp = mainGame->dInfo.lp[0] = mainGame->dInfo.lp[1] = duelOptions.startingLP;
	mainGame->dInfo.strLP[0] = mainGame->dInfo.strLP[1] = fmt::to_wstring(mainGame->dInfo.lp[0]);
	if(mainGame->dInfo.lp[0] >= 999999)
		mainGame->dInfo.strLP[0] = L"\u221E";
	else
		mainGame->dInfo.strLP[0] = fmt::to_wstring(mainGame->dInfo.lp[0]);
	if(mainGame->dInfo.lp[1] >= 999999)
		mainGame->dInfo.strLP[1] = L"\u221E";
	else
		mainGame->dInfo.strLP[1] = fmt::to_wstring(mainGame->dInfo.lp[1]);
	///////////kdiy///////////		
	mainGame->dInfo.selfnames = { mainGame->ebNickName->getText() };
	mainGame->dInfo.opponames = { L"" };
	mainGame->dInfo.player_type = 0;
	mainGame->dInfo.turn = 0;
	bool loaded = true;
	ReplayHeader rh;
	rh.id = REPLAY_YRP1;
	rh.version = CLIENT_VERSION;
	rh.flag = REPLAY_SINGLE_MODE | REPLAY_LUA64 | REPLAY_NEWREPLAY | REPLAY_64BIT_DUELFLAG;
	if(hand_test)
		rh.flag |= REPLAY_HAND_TEST;
	rh.seed = seed;
	bool saveReplay = !hand_test || gGameConfig->saveHandTest;
	if(saveReplay) {
		last_replay.BeginRecord(true, EPRO_TEXT("./replay/_LastReplay.yrp"));
		last_replay.WriteHeader(rh);
		//records the replay with the new system
		new_replay.BeginRecord();
		rh.id = REPLAY_YRPX;
		new_replay.WriteHeader(rh);
		replay_stream.clear();
	}
	if(hand_test) {
		script_name = "hand-test-mode";
		InitReplay();
		Deck playerdeck(gdeckManager->current_deck);
		if ((duelOptions.duelFlags & DUEL_PSEUDO_SHUFFLE) == 0)
			std::shuffle(playerdeck.main.begin(), playerdeck.main.end(), DuelClient::rnd);
		auto LoadDeck = [&](uint8_t team) {
			OCG_NewCardInfo card_info = { team, 0, 0, team, 0, 0, POS_FACEDOWN_DEFENSE };
			card_info.loc = LOCATION_DECK;
			last_replay.Write<uint32_t>(playerdeck.main.size(), false);
			for (int32_t i = (int32_t)playerdeck.main.size() - 1; i >= 0; --i) {
				card_info.code = playerdeck.main[i]->code;
				OCG_DuelNewCard(pduel, card_info);
				last_replay.Write<uint32_t>(playerdeck.main[i]->code, false);
			}
			card_info.loc = LOCATION_EXTRA;
			last_replay.Write<uint32_t>(playerdeck.extra.size(), false);
			for (int32_t i = (int32_t)playerdeck.extra.size() - 1; i >= 0; --i) {
				card_info.code = playerdeck.extra[i]->code;
				OCG_DuelNewCard(pduel, card_info);
				last_replay.Write<uint32_t>(playerdeck.extra[i]->code, false);
			}
		};
		LoadDeck(0);
		if (duelOptions.handTestNoOpponent) {
			last_replay.Write<uint32_t>(0, false);
			last_replay.Write<uint32_t>(0, false);
		} else {
			LoadDeck(1);
		}
		last_replay.Flush();
		const char cmd[] = "Debug.ReloadFieldEnd()";
		loaded = OCG_LoadScript(pduel, cmd, sizeof(cmd) - 1, " ");
	} else {
		if(open_file) {
			script_name = Utils::ToUTF8IfNeeded(open_file_name);
			if(!mainGame->LoadScript(pduel, script_name)) {
				script_name = Utils::ToUTF8IfNeeded(EPRO_TEXT("./puzzles/") + open_file_name);
				loaded = mainGame->LoadScript(pduel, script_name);
			}
		} else {
			script_name = BufferIO::EncodeUTF8s(mainGame->lstSinglePlayList->getListItem(mainGame->lstSinglePlayList->getSelected(), true));
			loaded = mainGame->LoadScript(pduel, script_name);
		}
		InitReplay();
	}
	if(!loaded) {
		OCG_DestroyDuel(pduel);
		pduel = nullptr;
		mainGame->dInfo.isSingleMode = false;
		mainGame->dInfo.isHandTest = false;
		open_file = false;
		last_replay.EndRecord();
		new_replay.EndRecord();
		std::unique_lock<std::mutex> lock(mainGame->gMutex);
		if(is_restarting) {
			mainGame->dInfo.isInDuel = false;
			mainGame->dInfo.isStarted = false;
			mainGame->dInfo.isSingleMode = false;
			mainGame->dInfo.isHandTest = false;
			if(!hand_test) {
				mainGame->closeDuelWindow = true;
				mainGame->closeDoneSignal.Wait(lock);
			}
			////kdiy////////
			//mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 80));
			mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 45));
			////kdiy////////	
			if(!hand_test) {
				mainGame->ShowElement(mainGame->wSinglePlay);
				mainGame->stTip->setVisible(false);
			}
			mainGame->SetMessageWindow();
			mainGame->device->setEventReceiver(&mainGame->menuHandler);
			if(hand_test) {
				mainGame->btnChainIgnore->setVisible(false);
				mainGame->btnChainAlways->setVisible(false);
				mainGame->btnChainWhenAvail->setVisible(false);
				mainGame->btnCancelOrFinish->setVisible(false);
				mainGame->btnShuffle->setVisible(false);
				mainGame->wChat->setVisible(false);
				mainGame->btnRestartSingle->setVisible(false);
				mainGame->wPhase->setVisible(false);
				mainGame->deckBuilder.Initialize(false);
			}
			if(exit_on_return)
				mainGame->device->closeDevice();
		} else
			////kdiy////////
			//mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 80));
			mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 45));
			////kdiy////////
		is_restarting = false;
		return 0;
	}
	mainGame->gMutex.lock();
	if(!hand_test && !is_restarting) {
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
	mainGame->gMutex.unlock();
	is_closing = false;
	is_continuing = true;
	int engFlag = 0;
	auto msg = CoreUtils::ParseMessages(pduel);
	for(auto& message : msg.packets)
		is_continuing = SinglePlayAnalyze(message) && is_continuing;
	if(is_continuing) {
		OCG_StartDuel(pduel);
	do {
		engFlag = OCG_DuelProcess(pduel);
		msg = CoreUtils::ParseMessages(pduel);
		for(auto& message : msg.packets) {
			if(message.message == MSG_WIN && hand_test)
				continue;
			is_continuing = SinglePlayAnalyze(message) && is_continuing;
		}
	} while(is_continuing && engFlag && mainGame->dInfo.curMsg != MSG_WIN);
	}
	OCG_DestroyDuel(pduel);
	pduel = nullptr;
	if(saveReplay && !is_restarting) {
		last_replay.EndRecord(0x1000);
		auto oldbuffer = last_replay.GetSerializedBuffer();
		ReplayPacket tmp{};
		tmp.message = OLD_REPLAY_MODE;
		tmp.data.swap(oldbuffer);
		new_replay.WritePacket(tmp);
		new_replay.EndRecord();
	}
	if(is_closing) {
		open_file = false;
		is_restarting = false;
		mainGame->gMutex.lock();
		////kdiy////////
		//mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 80));
		mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 45));
		////kdiy////////
		mainGame->gMutex.unlock();
		return 0;
	}
	gSoundManager->StopSounds();
	bool was_restarting = is_restarting;
	if(saveReplay && !was_restarting) {
		auto now = std::time(nullptr);
		std::unique_lock<std::mutex> lock(mainGame->gMutex);
		mainGame->ebRSName->setText(fmt::format(L"{:%Y-%m-%d %H-%M-%S}", *std::localtime(&now)).data());
		mainGame->wReplaySave->setText(gDataManager->GetSysString(1340).data());
		mainGame->PopupElement(mainGame->wReplaySave);
		mainGame->replaySignal.Wait(lock);
		if(mainGame->saveReplay)
			new_replay.SaveReplay(Utils::ToPathString(mainGame->ebRSName->getText()));
	}
	new_replay.Reset();
	last_replay.Reset();
	mainGame->gMutex.lock();
	mainGame->dField.Clear();
	mainGame->gMutex.unlock();
	if(!is_closing) {
		if(was_restarting || hand_test) {
			std::lock_guard<std::mutex> lock(mainGame->gMutex);
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
			if(was_restarting)
				goto restart;
		}
		std::unique_lock<std::mutex> lock(mainGame->gMutex);
		mainGame->dInfo.isInDuel = false;
		mainGame->dInfo.isStarted = false;
		mainGame->dInfo.isSingleMode = false;
		mainGame->dInfo.isHandTest = false;
		if(!hand_test) {
			mainGame->closeDuelWindow = true;
			mainGame->closeDoneSignal.Wait(lock);
		}
		////kdiy////////
		//mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 5, 295, 80));
		mainGame->btnLeaveGame->setRelativePosition(mainGame->Resize(205, 137, 295, 187));
		////kdiy////////		
		if(!hand_test) {
			mainGame->ShowElement(mainGame->wSinglePlay);
			mainGame->stTip->setVisible(false);
		}
		mainGame->SetMessageWindow();
		mainGame->device->setEventReceiver(&mainGame->menuHandler);
		if(hand_test) {
			mainGame->btnChainIgnore->setVisible(false);
			mainGame->btnChainAlways->setVisible(false);
			mainGame->btnChainWhenAvail->setVisible(false);
			mainGame->btnCancelOrFinish->setVisible(false);
			mainGame->btnShuffle->setVisible(false);
			mainGame->wChat->setVisible(false);
			mainGame->btnRestartSingle->setVisible(false);
			mainGame->wPhase->setVisible(false);
			mainGame->deckBuilder.Initialize(false);
		}
		if(exit_on_return)
			mainGame->device->closeDevice();
	}
	open_file = false;
	return 0;
}

bool SingleMode::SinglePlayAnalyze(CoreUtils::Packet packet) {
	auto Analyze = [&packet]()->bool {
		DuelClient::answered = false;
		return DuelClient::ClientAnalyze((char*)packet.data.data(), packet.data.size());
	};
	auto Data = [&packet]()->char* {
		return (char*)(packet.data.data() + sizeof(uint8_t));
	};
	int player;
	replay_stream.clear();
	if(is_closing || !is_continuing)
		return false;
	mainGame->dInfo.curMsg = packet.message;
	bool record = true;
	bool record_last = false;
	switch(mainGame->dInfo.curMsg) {
		case MSG_RETRY:	{
			std::unique_lock<std::mutex> lock(mainGame->gMutex);
			mainGame->stMessage->setText(gDataManager->GetSysString(1434).data());
			mainGame->PopupElement(mainGame->wMessage);
			mainGame->actionSignal.Wait(lock);
			return false;
		}
		case MSG_HINT: {
			char* pbuf = Data();
			int type = BufferIO::Read<uint8_t>(pbuf);
			int player = BufferIO::Read<uint8_t>(pbuf);
			/*uint64_t data = BufferIO::Read<uint64_t>(pbuf);*/
			if(player == 0 || type >= HINT_SKILL)
				Analyze();
			if(type > 0 && type < 6 && type != 4)
				record = false;
			break;
		}
		case MSG_AI_NAME:
		case MSG_SHOW_HINT: {
			char* pbuf = Data();
			int len = BufferIO::Read<uint16_t>(pbuf);
			if((len + 1) != packet.data.size() - (sizeof(uint8_t) + sizeof(uint16_t)))
				break;
			pbuf[len] = 0;
			if(packet.message == MSG_AI_NAME) {
				mainGame->dInfo.opponames[0] = BufferIO::DecodeUTF8s(pbuf);
			} else {
				std::unique_lock<std::mutex> lock(mainGame->gMutex);
				mainGame->stMessage->setText(BufferIO::DecodeUTF8s(pbuf).data());
				mainGame->PopupElement(mainGame->wMessage);
				mainGame->actionSignal.Wait(lock);
			}
			break;
		}
		case MSG_SELECT_BATTLECMD:
		case MSG_SELECT_IDLECMD: {
			record = false;
			SinglePlayRefresh();
			if(!Analyze())
				singleSignal.Wait();
			break;
		}
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
			if(mainGame->dInfo.curMsg == MSG_SELECT_CHAIN || mainGame->dInfo.curMsg == MSG_NEW_TURN) {
				SinglePlayRefresh(0, LOCATION_MZONE);
				SinglePlayRefresh(1, LOCATION_MZONE);
				SinglePlayRefresh(0, LOCATION_SZONE);
				SinglePlayRefresh(1, LOCATION_SZONE);
				record_last = true;
			}
			if(!Analyze())
				singleSignal.Wait();
			break;
		}
		default: {
			Analyze();
			break;
		}
	}
	char* pbuf = Data();
	switch(mainGame->dInfo.curMsg) {
		case MSG_SHUFFLE_DECK: {
			player = BufferIO::Read<uint8_t>(pbuf);
			SinglePlayRefresh(player, LOCATION_DECK, 0x2181fff);
			break;
		}
		case MSG_SWAP_GRAVE_DECK: {
			player = BufferIO::Read<uint8_t>(pbuf);
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
			player = BufferIO::Read<uint8_t>(pbuf);
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
			std::lock_guard<std::mutex> lock(mainGame->gMutex);
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
	auto buff = OCG_DuelQueryLocation(pduel, &len, { flag, player, location });
	if(len == 0)
		return;
	buffer.resize(buffer.size() + len);
	memcpy(buffer.data(), buff, len);
	mainGame->gMutex.lock();
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(player), location, (char*)buffer.data());
	mainGame->gMutex.unlock();
	buffer.insert(buffer.begin(), location);
	buffer.insert(buffer.begin(), player);
	replay_stream.emplace_back(MSG_UPDATE_DATA, (char*)buffer.data(), buffer.size());
}
void SingleMode::SinglePlayRefreshSingle(uint8_t player, uint8_t location, uint8_t sequence, uint32_t flag) {
	std::vector<uint8_t> buffer;
	uint32_t len = 0;
	auto buff = OCG_DuelQuery(pduel, &len, { flag, player, location, sequence });
	if(buff == nullptr)
		return;
	buffer.resize(buffer.size() + len);
	memcpy(buffer.data(), buff, len);
	mainGame->gMutex.lock();
	mainGame->dField.UpdateCard(mainGame->LocalPlayer(player), location, sequence, (char*)buffer.data());
	mainGame->gMutex.unlock();
	buffer.insert(buffer.begin(), sequence);
	buffer.insert(buffer.begin(), location);
	buffer.insert(buffer.begin(), player);
	replay_stream.emplace_back(MSG_UPDATE_CARD, (char*)buffer.data(), buffer.size());
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
