#include <algorithm>
#include <fmt/printf.h>
#include <fmt/chrono.h>
#include "config.h"
#if EDOPRO_WINDOWS
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#if !EDOPRO_ANDROID
#include <sys/types.h>
#include <signal.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/wait.h>
#endif
#endif
#include "game_config.h"
#include <irrlicht.h>
#include "duelclient.h"
#include "client_card.h"
#include "materials.h"
#include "image_manager.h"
#include "single_mode.h"
#include "game.h"
#include "replay.h"
#include "replay_mode.h"
#include "sound_manager.h"
#include "CGUIImageButton/CGUIImageButton.h"
#include "progressivebuffer.h"
#include "utils.h"
#include "porting.h"

#define DEFAULT_DUEL_RULE 5
namespace ygo {

uint32_t DuelClient::connect_state = 0;
std::vector<uint8_t> DuelClient::response_buf;
uint32_t DuelClient::watching = 0;
uint8_t DuelClient::selftype = 0;
bool DuelClient::is_host = false;
bool DuelClient::is_local_host = false;
std::atomic<bool> DuelClient::answered{ false };
event_base* DuelClient::client_base = nullptr;
bufferevent* DuelClient::client_bev = nullptr;
bool DuelClient::is_closing = false;
uint64_t DuelClient::select_hint = 0;
std::wstring DuelClient::event_string;
RNG::mt19937 DuelClient::rnd;

ReplayStream DuelClient::replay_stream;
Replay DuelClient::last_replay;
bool DuelClient::is_swapping = false;
bool DuelClient::stop_threads{ true };
std::deque<std::vector<uint8_t>> DuelClient::to_analyze;
epro::mutex DuelClient::analyzeMutex;
epro::mutex DuelClient::to_analyze_mutex;
epro::thread DuelClient::parsing_thread;
epro::thread DuelClient::client_thread;
epro::condition_variable DuelClient::cv;

bool DuelClient::is_refreshing = false;
int DuelClient::match_kill = 0;
std::vector<HostPacket> DuelClient::hosts;
std::set<std::pair<uint32_t, uint16_t>> DuelClient::remotes;
event* DuelClient::resp_event = 0;

uint32_t DuelClient::temp_ip = 0;
uint16_t DuelClient::temp_port = 0;
uint16_t DuelClient::temp_ver = 0;
bool DuelClient::try_needed = false;

std::pair<uint32_t, uint16_t> DuelClient::ResolveServer(epro::stringview address, uint16_t port) {
	uint32_t remote_addr = inet_addr(address.data());
	if (remote_addr == static_cast<uint32_t>(-1)) {
		evutil_addrinfo hints{};
		evutil_addrinfo *answer = nullptr;
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = EVUTIL_AI_ADDRCONFIG;
		if(evutil_getaddrinfo(address.data(), fmt::to_string(port).data(), &hints, &answer) != 0)
			throw std::runtime_error("Host not resolved");
		auto* in_answer = reinterpret_cast<sockaddr_in*>(answer->ai_addr);
		remote_addr = in_answer->sin_addr.s_addr;
		evutil_freeaddrinfo(answer);
	}
	return { remote_addr, port };
}

bool DuelClient::StartClient(uint32_t ip, uint16_t port, uint32_t gameid, bool create_game) {
	if(connect_state)
		return false;
	client_base = event_base_new();
	if(!client_base)
		return false;
	sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = ip;
	sin.sin_port = htons(port);
	client_bev = bufferevent_socket_new(client_base, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
	bufferevent_setcb(client_bev, ClientRead, nullptr, ClientEvent, (void*)create_game);
	bufferevent_enable(client_bev, EV_READ);
	temp_ip = ip;
	temp_port = port;
	if (bufferevent_socket_connect(client_bev, (sockaddr*)&sin, sizeof(sin)) < 0) {
		bufferevent_free(client_bev);
		event_base_free(client_base);
		client_bev = 0;
		client_base = 0;
		return false;
	}
	connect_state = 0x1;
	rnd.seed(time(0));
	if(!create_game) {
		timeval timeout = {5, 0};
		event* timeout_event = event_new(client_base, 0, EV_TIMEOUT, ConnectTimeout, 0);
		event_add(timeout_event, &timeout);
	}
	mainGame->dInfo.secret.game_id = gameid;
	mainGame->dInfo.secret.server_port = port;
	mainGame->dInfo.secret.server_address = ip;
	mainGame->dInfo.isCatchingUp = false;
	mainGame->dInfo.checkRematch = false;
	mainGame->frameSignal.SetNoWait(true);
	if(client_thread.joinable())
		client_thread.join();
	mainGame->frameSignal.SetNoWait(false);
	stop_threads = false;
	parsing_thread = epro::thread(ParserThread);
	client_thread = epro::thread(ClientThread);
	return true;
}
void DuelClient::ConnectTimeout(evutil_socket_t fd, short events, void* arg) {
	(void)fd;
	(void)events;
	(void)arg;
	if(connect_state & 0x7)
		return;
	if(!is_closing) {
		temp_ver = 0;
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->btnCreateHost->setEnabled(mainGame->coreloaded);
		mainGame->btnJoinHost->setEnabled(true);
		mainGame->btnJoinCancel->setEnabled(true);
		if(mainGame->isHostingOnline) {
			if(!mainGame->wRoomListPlaceholder->isVisible())
				mainGame->ShowElement(mainGame->wRoomListPlaceholder);
		} else {
			if(!mainGame->wLanWindow->isVisible())
				mainGame->ShowElement(mainGame->wLanWindow);
		}
		mainGame->PopupMessage(gDataManager->GetSysString(1400));
	}
	event_base_loopbreak(client_base);
}
void DuelClient::StopClient(bool is_exiting) {
	mainGame->frameSignal.SetNoWait(true);
	if((connect_state & 0x7) != 0) {
		is_closing = is_exiting;
		to_analyze_mutex.lock();
		to_analyze.clear();
		event_base_loopbreak(client_base);
		to_analyze_mutex.unlock();
#if EDOPRO_LINUX || EDOPRO_MACOS
		for(auto& pid : mainGame->gBot.windbotsPids) {
			kill(pid, SIGKILL);
			(void)waitpid(pid, nullptr, 0);
		}
		mainGame->gBot.windbotsPids.clear();
#endif
		if(!is_closing) {

		}
	}
	if(client_thread.joinable())
		client_thread.join();
	mainGame->frameSignal.SetNoWait(false);
}
void DuelClient::ClientRead(bufferevent* bev, void* ctx) {
	(void)ctx;
	evbuffer* input = bufferevent_get_input(bev);
	size_t len = evbuffer_get_length(input);
	uint16_t packet_len = 0;
	while(true) {
		if(len < 2)
			return;
		evbuffer_copyout(input, &packet_len, 2);
		if(len < packet_len + 2u)
			return;
		evbuffer_drain(input, 2);
		std::vector<uint8_t> duel_client_read(packet_len);
		evbuffer_remove(input, duel_client_read.data(), packet_len);
		if(packet_len)
			HandleSTOCPacketLanSync(std::move(duel_client_read));
		len = evbuffer_get_length(input);
	}
}

#define INTERNAL_HANDLE_CONNECTION_END 0

void DuelClient::ClientEvent(bufferevent *bev, short events, void *ctx) {
	(void)bev;
	if (events & BEV_EVENT_CONNECTED) {
		bool create_game = (size_t)ctx != 0;
		CTOS_PlayerInfo cspi;
		BufferIO::EncodeUTF16(mainGame->ebNickName->getText(), cspi.name, 20);
		SendPacketToServer(CTOS_PLAYER_INFO, cspi);
		if(create_game) {
#define TOI(what, from, def) try { what = std::stoi(from);  }\
catch(...) { what = def; }
			CTOS_CreateGame cscg;
			mainGame->dInfo.secret.game_id = 0;
			BufferIO::EncodeUTF16(mainGame->ebServerName->getText(), cscg.name, 20);
			BufferIO::EncodeUTF16(mainGame->ebServerPass->getText(), cscg.pass, 20);
			mainGame->dInfo.secret.pass = mainGame->ebServerPass->getText();
			cscg.info.rule = mainGame->cbRule->getSelected();
			cscg.info.mode = 0;
			TOI(cscg.info.start_hand, mainGame->ebStartHand->getText(), 5);
			TOI(cscg.info.start_lp, mainGame->ebStartLP->getText(), 8000);
			TOI(cscg.info.draw_count, mainGame->ebDrawCount->getText(), 1);
			TOI(cscg.info.time_limit, mainGame->ebTimeLimit->getText(), 0);
			cscg.info.lflist = gGameConfig->lastlflist = mainGame->cbHostLFList->getItemData(mainGame->cbHostLFList->getSelected());
			cscg.info.duel_rule = 0;
			cscg.info.duel_flag_low = mainGame->duel_param & 0xffffffff;
			cscg.info.duel_flag_high = (mainGame->duel_param >> 32) & 0xffffffff;
			cscg.info.no_check_deck_content = mainGame->chkNoCheckDeckContent->isChecked();
			cscg.info.no_shuffle_deck = mainGame->chkNoShuffleDeck->isChecked();
			cscg.info.handshake = SERVER_HANDSHAKE;
			cscg.info.version = { EXPAND_VERSION(CLIENT_VERSION) };
			TOI(cscg.info.team1, mainGame->ebTeam1->getText(), 1);
			TOI(cscg.info.team2, mainGame->ebTeam2->getText(), 1);
			TOI(cscg.info.best_of, mainGame->ebBestOf->getText(), 1);
			static constexpr DeckSizes nolimit_deck_sizes{ {0,999},{0,999},{0,999} };
			auto& sizes = cscg.info.sizes;
			if(mainGame->chkNoCheckDeckSize->isChecked()) {
				sizes = nolimit_deck_sizes;
			} else {
				TOI(sizes.main.min, mainGame->ebMainMin->getText(), 40);
				TOI(sizes.main.max, mainGame->ebMainMax->getText(), 60);
				TOI(sizes.extra.min, mainGame->ebExtraMin->getText(), 0);
				TOI(sizes.extra.max, mainGame->ebExtraMax->getText(), 15);
				TOI(sizes.side.min, mainGame->ebSideMin->getText(), 0);
				TOI(sizes.side.max, mainGame->ebSideMax->getText(), 15);
			}
#undef TOI
			if(mainGame->btnRelayMode->isPressed())
				cscg.info.duel_flag_low |= DUEL_RELAY;
			if(cscg.info.no_shuffle_deck)
				cscg.info.duel_flag_low |= DUEL_PSEUDO_SHUFFLE;
			cscg.info.forbiddentypes = mainGame->forbiddentypes;
			cscg.info.extra_rules = mainGame->extra_rules;
			if(mainGame->ebHostNotes->isVisible()) {
				BufferIO::EncodeUTF8(mainGame->ebHostNotes->getText(), cscg.notes, 200);
			}
			SendPacketToServer(CTOS_CREATE_GAME, cscg);
		} else {
			CTOS_JoinGame csjg;
			if (temp_ver)
				csjg.version = temp_ver;
			else {
				csjg.version = PRO_VERSION;
				csjg.version2 = { EXPAND_VERSION(CLIENT_VERSION) };
			}
			csjg.gameid = mainGame->dInfo.secret.game_id;
			BufferIO::EncodeUTF16(mainGame->dInfo.secret.pass.data(), csjg.pass, 20);
			SendPacketToServer(CTOS_JOIN_GAME, csjg);
		}
		connect_state |= 0x2;
	} else if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
		if(!is_closing) {
			std::vector<uint8_t> tmp;
			tmp.resize(2);
			tmp[0] = INTERNAL_HANDLE_CONNECTION_END;
			tmp[1] = (events & BEV_EVENT_ERROR) == 0;
			to_analyze_mutex.lock();
			to_analyze.push_back(std::move(tmp));
			to_analyze_mutex.unlock();
			cv.notify_one();
		}
		event_base_loopexit(client_base, 0);
	}
}

void DuelClient::ClientThread() {
	Utils::SetThreadName("ClientThread");
	event_base_dispatch(client_base);
	to_analyze_mutex.lock();
	stop_threads = true;
	cv.notify_one();
	to_analyze_mutex.unlock();
	parsing_thread.join();
	bufferevent_free(client_bev);
	event_base_free(client_base);
	connect_state = 0;
	client_bev = 0;
	client_base = 0;
}

void DuelClient::ParserThread() {
	Utils::SetThreadName("ParserThread");
	while(true) {
		std::unique_lock<epro::mutex> lck(to_analyze_mutex);
		while(to_analyze.empty()) {
			if(stop_threads)
				return;
			cv.wait(lck);
		}
		auto pkt = std::move(to_analyze.front());
		to_analyze.pop_front();
		lck.unlock();
		HandleSTOCPacketLanAsync(pkt);
	}
}

void DuelClient::HandleSTOCPacketLanSync(std::vector<uint8_t>&& data) {
	uint8_t pktType = data[0];
	if(pktType != STOC_CHAT && pktType != STOC_CHAT_2) {
		to_analyze_mutex.lock();
		to_analyze.push_back(std::move(data));
		to_analyze_mutex.unlock();
		cv.notify_one();
		return;
	}
	if(mainGame->dInfo.isCatchingUp)
		return;
	auto* pdata = data.data() + 1;
	switch(pktType) {
		case STOC_CHAT_2: {
			auto pkt = BufferIO::getStruct<STOC_Chat2>(pdata, data.size());
			if(pkt.type == STOC_Chat2::PTYPE_DUELIST && (mainGame->dInfo.player_type >= 7 || !pkt.is_team) && mainGame->tabSettings.chkIgnoreOpponents->isChecked())
				return;
			if(pkt.type == STOC_Chat2::PTYPE_OBS && mainGame->tabSettings.chkIgnoreSpectators->isChecked())
				return;
			wchar_t name[20];
			BufferIO::DecodeUTF16(pkt.client_name, name, 20);
			wchar_t msg[256];
			BufferIO::DecodeUTF16(pkt.msg, msg, 256);
			std::lock_guard<epro::mutex> lock(mainGame->gMutex);
			mainGame->AddChatMsg(name, msg, pkt.type);
			break;
		}
	}
}


void DuelClient::HandleSTOCPacketLanAsync(const std::vector<uint8_t>& data) {
	auto* pdata = data.data();
	auto len = data.size();
	uint8_t pktType = BufferIO::Read<uint8_t>(pdata);
	switch(pktType) {
	case INTERNAL_HANDLE_CONNECTION_END: {
		bool iseof = !!BufferIO::Read<uint8_t>(pdata);
		mainGame->dInfo.isInLobby = false;
		if(connect_state == 0x1) {
			temp_ver = 0;
			std::lock_guard<epro::mutex> lock(mainGame->gMutex);
			mainGame->btnCreateHost->setEnabled(mainGame->coreloaded);
			mainGame->btnJoinHost->setEnabled(true);
			mainGame->btnJoinCancel->setEnabled(true);
			if(mainGame->isHostingOnline) {
				if(!mainGame->wCreateHost->isVisible() && !mainGame->wRoomListPlaceholder->isVisible())
					mainGame->ShowElement(mainGame->wRoomListPlaceholder);
			} else {
				if(!mainGame->wLanWindow->isVisible())
					mainGame->ShowElement(mainGame->wLanWindow);
			}
			mainGame->PopupMessage(gDataManager->GetSysString(1400));
		} else if(connect_state == 0x7) {
			if(!mainGame->dInfo.isInDuel && !mainGame->is_building) {
				std::lock_guard<epro::mutex> lock(mainGame->gMutex);
				mainGame->btnCreateHost->setEnabled(mainGame->coreloaded);
				mainGame->btnJoinHost->setEnabled(true);
				mainGame->btnJoinCancel->setEnabled(true);
				mainGame->HideElement(mainGame->wCreateHost);
				mainGame->HideElement(mainGame->wHostPrepare);
				mainGame->HideElement(mainGame->wHostPrepareL);
				mainGame->HideElement(mainGame->wHostPrepareR);
				mainGame->HideElement(mainGame->gBot.window);
				if(mainGame->isHostingOnline) {
					mainGame->ShowElement(mainGame->wRoomListPlaceholder);
				} else {
					mainGame->ShowElement(mainGame->wLanWindow);
				}
				mainGame->wChat->setVisible(false);
				if(iseof)
					mainGame->PopupMessage(gDataManager->GetSysString(1401));
				else mainGame->PopupMessage(gDataManager->GetSysString(1402));
			} else {
				gSoundManager->StopSounds();
				if(mainGame->dInfo.isStarted) {
					ReplayPrompt(true);
				}
				std::unique_lock<epro::mutex> lock(mainGame->gMutex);
				mainGame->PopupMessage(gDataManager->GetSysString(1502));
				mainGame->btnCreateHost->setEnabled(mainGame->coreloaded);
				mainGame->btnJoinHost->setEnabled(true);
				mainGame->btnJoinCancel->setEnabled(true);
				mainGame->stTip->setVisible(false);
				mainGame->stHintMsg->setVisible(false);
				mainGame->closeDuelWindow = true;
				mainGame->closeDoneSignal.Wait(lock);
				mainGame->dInfo.checkRematch = false;
				mainGame->dInfo.isInDuel = false;
				mainGame->dInfo.isStarted = false;
				mainGame->dField.Clear();
				mainGame->is_building = false;
				mainGame->device->setEventReceiver(&mainGame->menuHandler);
				if(mainGame->isHostingOnline) {
					mainGame->ShowElement(mainGame->wRoomListPlaceholder);
				} else {
					mainGame->ShowElement(mainGame->wLanWindow);
				}
				mainGame->SetMessageWindow();
			}
		}
		break;
	}
	case STOC_GAME_MSG: {
		analyzeMutex.lock();
		answered = false;
		ClientAnalyze(pdata, static_cast<uint32_t>(len - 1));
		analyzeMutex.unlock();
		break;
	}
	case STOC_ERROR_MSG: {
		auto _pkt = BufferIO::getStruct<STOC_ErrorMsg>(pdata, len);
		switch(_pkt.type) {
		case ERROR_TYPE::JOINERROR: {
			auto pkt = BufferIO::getStruct<JoinError>(pdata, len);
			temp_ver = 0;
			std::lock_guard<epro::mutex> lock(mainGame->gMutex);
			if(mainGame->isHostingOnline) {
#define HIDE_AND_CHECK(obj) if(obj->isVisible()) mainGame->HideElement(obj);
				HIDE_AND_CHECK(mainGame->wCreateHost);
				HIDE_AND_CHECK(mainGame->wRules);
#undef HIDE_AND_CHECK
				mainGame->ShowElement(mainGame->wRoomListPlaceholder);
			} else {
				mainGame->btnCreateHost->setEnabled(mainGame->coreloaded);
				mainGame->btnJoinHost->setEnabled(true);
				mainGame->btnJoinCancel->setEnabled(true);
			}
			int stringid = 1406;
			switch(pkt.error) {
				case JoinError::JERR_UNABLE:	stringid--;
					/*fallthrough*/
				case JoinError::JERR_PASSWORD:	stringid--;
					/*fallthrough*/
				case JoinError::JERR_REFUSED:	stringid--;
					/*fallthrough*/
			}
			if(stringid < 1406)
				mainGame->PopupMessage(gDataManager->GetSysString(stringid));
			connect_state |= 0x100;
			event_base_loopbreak(client_base);
			break;
		}
		case ERROR_TYPE::DECKERROR: {
			auto pkt = BufferIO::getStruct<DeckError>(pdata, len);
			std::lock_guard<epro::mutex> lock(mainGame->gMutex);
			int mainmin = 40, mainmax = 60, extramax = 15, sidemax = 15;
			uint32_t code = 0, curcount = 0;
			DeckError::DERR_TYPE flag = DeckError::NONE;
			if(mainGame->dInfo.compat_mode) {
				curcount = pkt.code;
				code = pkt.code & 0xFFFFFFF;
				flag = static_cast<DeckError::DERR_TYPE>(pkt.code >> 28);
			} else {
				code = pkt.code;
				flag = pkt.type;
				mainmin = pkt.count.minimum;
				mainmax = extramax = sidemax = pkt.count.maximum;
				curcount = pkt.count.current;
			}
			std::wstring text;
			switch(flag) {
			case DeckError::LFLIST: {
				text = epro::sprintf(gDataManager->GetSysString(1407), gDataManager->GetName(code));
				break;
			}
			case DeckError::OCGONLY: {
				text = epro::sprintf(gDataManager->GetSysString(1413), gDataManager->GetName(code));
				break;
			}
			case DeckError::TCGONLY: {
				text = epro::sprintf(gDataManager->GetSysString(1414), gDataManager->GetName(code));
				break;
			}
			case DeckError::UNKNOWNCARD: {
				text = epro::sprintf(gDataManager->GetSysString(1415), gDataManager->GetName(code), code);
				break;
			}
			case DeckError::CARDCOUNT: {
				text = epro::sprintf(gDataManager->GetSysString(1416), gDataManager->GetName(code));
				break;
			}
			case DeckError::MAINCOUNT: {
				text = epro::sprintf(gDataManager->GetSysString(1417), mainmin, mainmax, curcount);
				break;
			}
			case DeckError::EXTRACOUNT: {
				if(curcount > 0)
					text = epro::sprintf(gDataManager->GetSysString(1418), extramax, curcount);
				else
					text = gDataManager->GetSysString(1420).data();
				break;
			}
			case DeckError::SIDECOUNT: {
				text = epro::sprintf(gDataManager->GetSysString(1419), sidemax, curcount);
				break;
			}
			case DeckError::FORBTYPE: {
				text = gDataManager->GetSysString(1421).data();
				break;
			}
			case DeckError::UNOFFICIALCARD: {
				text = epro::sprintf(gDataManager->GetSysString(1422), gDataManager->GetName(code));
				break;
			}
			case DeckError::INVALIDSIZE: {
				text = gDataManager->GetSysString(1425).data();
				break;
			}
			case DeckError::TOOMANYLEGENDS: {
				text = gDataManager->GetSysString(1426).data();
				break;
			}
			case DeckError::TOOMANYSKILLS: {
				text = gDataManager->GetSysString(1427).data();
				break;
			}
			default: {
				text = gDataManager->GetSysString(1406).data();
				break;
			}
			}
			mainGame->PopupMessage(text);
			mainGame->cbDeckSelect->setEnabled(true);
			if(mainGame->dInfo.team1 + mainGame->dInfo.team2 > 2)
				mainGame->btnHostPrepDuelist->setEnabled(true);
			break;
		}
		case ERROR_TYPE::SIDEERROR: {
			std::lock_guard<epro::mutex> lock(mainGame->gMutex);
			mainGame->PopupMessage(gDataManager->GetSysString(1408));
			break;
		}
		case ERROR_TYPE::VERERROR:
		case ERROR_TYPE::VERERROR2: {
			if(temp_ver || (_pkt.type == ERROR_TYPE::VERERROR2)) {
				temp_ver = 0;
				std::lock_guard<epro::mutex> lock(mainGame->gMutex);
				mainGame->btnCreateHost->setEnabled(mainGame->coreloaded);
				mainGame->btnJoinHost->setEnabled(true);
				mainGame->btnJoinCancel->setEnabled(true);
				mainGame->btnHostConfirm->setEnabled(true);
				mainGame->btnHostCancel->setEnabled(true);
				if(_pkt.type == ERROR_TYPE::VERERROR2) {
					auto version = BufferIO::getStruct<VersionError>(pdata, len).version;
					mainGame->PopupMessage(epro::format(gDataManager->GetSysString(1423),
													   version.client.major, version.client.minor,
													   version.core.major, version.core.minor));
				} else {
					mainGame->PopupMessage(epro::sprintf(gDataManager->GetSysString(1411), _pkt.code >> 12, (_pkt.code >> 4) & 0xff, _pkt.code & 0xf));
				}
				if(mainGame->isHostingOnline) {
#define HIDE_AND_CHECK(obj) if(obj->isVisible()) mainGame->HideElement(obj);
					HIDE_AND_CHECK(mainGame->wCreateHost);
					HIDE_AND_CHECK(mainGame->wRules);
#undef HIDE_AND_CHECK
					mainGame->ShowElement(mainGame->wRoomListPlaceholder);
				} else {
					mainGame->btnCreateHost->setEnabled(mainGame->coreloaded);
				}
			} else {
				temp_ver = _pkt.code;
				try_needed = true;
			}
			connect_state |= 0x100;
			event_base_loopbreak(client_base);
			break;
		}
		}
		break;
	}
	case STOC_SELECT_HAND: {
		mainGame->wHand->setVisible(true);
		break;
	}
	case STOC_SELECT_TP: {
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->PopupElement(mainGame->wFTSelect);
		break;
	}
	case STOC_HAND_RESULT: {
		if(mainGame->dInfo.isCatchingUp)
			break;
		auto pkt = BufferIO::getStruct<STOC_HandResult>(pdata, len);
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->stHintMsg->setVisible(false);
		mainGame->showcardcode = (pkt.res1 - 1) + ((pkt.res2 - 1) << 16);
		mainGame->showcarddif = 50;
		mainGame->showcardp = 0;
		mainGame->showcard = 100;
		mainGame->WaitFrameSignal(60, lock);
		break;
	}
	case STOC_TP_RESULT: {
		break;
	}
	case STOC_CHANGE_SIDE: {
		gSoundManager->StopSounds();
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->dInfo.checkRematch = false;
		mainGame->dInfo.isInLobby = false;
		mainGame->dInfo.isInDuel = false;
		mainGame->dInfo.isStarted = false;
		mainGame->dField.Clear();
		mainGame->is_building = true;
		mainGame->is_siding = true;
		mainGame->wChat->setVisible(false);
		mainGame->wPhase->setVisible(false);
		mainGame->wDeckEdit->setVisible(false);
		mainGame->wFilter->setVisible(false);
		mainGame->wSort->setVisible(false);
		mainGame->stTip->setVisible(false);
		mainGame->btnSideOK->setVisible(true);
		mainGame->btnSideShuffle->setVisible(true);
		mainGame->btnSideSort->setVisible(true);
		mainGame->btnSideReload->setVisible(true);
		if(mainGame->wQuery->isVisible())
			mainGame->HideElement(mainGame->wQuery);
		if(mainGame->wPosSelect->isVisible())
			mainGame->HideElement(mainGame->wPosSelect);
		if(mainGame->wCardSelect->isVisible())
			mainGame->HideElement(mainGame->wCardSelect);
		if(mainGame->wCardDisplay->isVisible())
			mainGame->HideElement(mainGame->wCardDisplay);
		if(mainGame->wANNumber->isVisible())
			mainGame->HideElement(mainGame->wANNumber);
		if(mainGame->wANCard->isVisible())
			mainGame->HideElement(mainGame->wANCard);
		if(mainGame->dInfo.player_type < 7)
			mainGame->btnLeaveGame->setVisible(false);
		mainGame->btnSpectatorSwap->setVisible(false);
		mainGame->btnChainIgnore->setVisible(false);
		mainGame->btnChainAlways->setVisible(false);
		mainGame->btnChainWhenAvail->setVisible(false);
		mainGame->btnCancelOrFinish->setVisible(false);
		mainGame->deckBuilder.result_string = L"0";
		mainGame->deckBuilder.results.clear();
		mainGame->deckBuilder.hovered_code = 0;
		mainGame->deckBuilder.is_draging = false;
		gdeckManager->pre_deck = gdeckManager->sent_deck;
		mainGame->deckBuilder.SetCurrentDeck(gdeckManager->sent_deck);
		mainGame->device->setEventReceiver(&mainGame->deckBuilder);
		mainGame->dInfo.isFirst = (mainGame->dInfo.player_type < mainGame->dInfo.team1) || (mainGame->dInfo.player_type >=7);
		mainGame->dInfo.isTeam1 = mainGame->dInfo.isFirst;
		mainGame->SetMessageWindow();
		break;
	}
	case STOC_WAITING_SIDE:
	case STOC_WAITING_REMATCH: {
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->dField.Clear();
		mainGame->stHintMsg->setText(gDataManager->GetSysString(pktType == STOC_WAITING_SIDE ?  1409 : 1424).data());
		mainGame->stHintMsg->setVisible(true);
		break;
	}
	case STOC_CREATE_GAME: {
		mainGame->dInfo.secret.game_id = BufferIO::getStruct<STOC_CreateGame>(pdata, len).gameid;
		break;
	}
	case STOC_JOIN_GAME: {
		temp_ver = 0;
		auto pkt = BufferIO::getStruct<STOC_JoinGame>(pdata, len);
		mainGame->dInfo.isInLobby = true;
		mainGame->dInfo.compat_mode = pkt.info.handshake != SERVER_HANDSHAKE;
		mainGame->dInfo.legacy_race_size = mainGame->dInfo.compat_mode || (pkt.info.version.core.major < 10);
		if(mainGame->dInfo.compat_mode) {
			pkt.info.duel_flag_low = 0;
			pkt.info.duel_flag_high = 0;
			pkt.info.forbiddentypes = 0;
			pkt.info.extra_rules = 0;
			pkt.info.best_of = 1;
			pkt.info.team1 = 1;
			pkt.info.team2 = 1;
			if(pkt.info.mode == MODE_MATCH) {
				pkt.info.best_of = 3;
			}
			if(pkt.info.mode == MODE_TAG) {
				pkt.info.team1 = 2;
				pkt.info.team2 = 2;
			}
#define CHK(rule) case rule : pkt.info.duel_flag_low = DUEL_MODE_MR##rule;break;
			switch(pkt.info.duel_rule) {
				CHK(1)
				CHK(2)
				CHK(3)
				CHK(4)
				CHK(5)
			}
#undef CHK
		}
		uint64_t params = (pkt.info.duel_flag_low | ((uint64_t)pkt.info.duel_flag_high) << 32);
		mainGame->dInfo.duel_params = params;
		mainGame->dInfo.isRelay = params & DUEL_RELAY;
		params &= ~DUEL_RELAY;
		pkt.info.no_shuffle_deck = (pkt.info.no_shuffle_deck != 0) || ((params & DUEL_PSEUDO_SHUFFLE) != 0);
		params &= ~DUEL_PSEUDO_SHUFFLE;
		mainGame->dInfo.team1 = pkt.info.team1;
		mainGame->dInfo.team2 = pkt.info.team2;
		mainGame->dInfo.best_of = pkt.info.best_of;
		std::wstring str, strR, strL;
		str.append(epro::format(L"{}{}\n", gDataManager->GetSysString(1226), gdeckManager->GetLFListName(pkt.info.lflist)));
		str.append(epro::format(L"{}{}\n", gDataManager->GetSysString(1225), gDataManager->GetSysString(1900 + pkt.info.rule)));
		if(mainGame->dInfo.compat_mode)
			str.append(epro::format(L"{}{}\n", gDataManager->GetSysString(1227), gDataManager->GetSysString(1244 + pkt.info.mode)));
		else {
			str.append(epro::format(L"{}{} {}{}\n", gDataManager->GetSysString(1227), gDataManager->GetSysString(1381), mainGame->dInfo.best_of, mainGame->dInfo.isRelay ? L" Relay" : L""));
		}
		if(pkt.info.time_limit) {
			str.append(epro::format(L"{}{}\n", gDataManager->GetSysString(1237), pkt.info.time_limit));
		}
		str.append(L"==========\n");
		str.append(epro::format(L"{}{}\n", gDataManager->GetSysString(1231), pkt.info.start_lp));
		str.append(epro::format(L"{}{}\n", gDataManager->GetSysString(1232), pkt.info.start_hand));
		str.append(epro::format(L"{}{}\n", gDataManager->GetSysString(1233), pkt.info.draw_count));
		int rule;
		mainGame->dInfo.duel_field = mainGame->GetMasterRule(params & ~DUEL_TCG_SEGOC_NONPUBLIC, pkt.info.forbiddentypes, &rule);
		if(mainGame->dInfo.compat_mode)
			rule = pkt.info.duel_rule;
		if (rule >= 6) {
			if(params == DUEL_MODE_SPEED) {
				str.append(epro::format(L"*{}\n", gDataManager->GetSysString(1258)));
			} else if(params == DUEL_MODE_RUSH) {
				str.append(epro::format(L"*{}\n", gDataManager->GetSysString(1259)));
			} else if(params  == DUEL_MODE_GOAT) {
				str.append(epro::format(L"*{}\n", gDataManager->GetSysString(1248)));
			} else {
				uint64_t filter = 0x100;
				for(uint32_t i = 0; filter && i < sizeofarr(mainGame->chkCustomRules); ++i, filter <<= 1)
					if(params & filter) {
						strR.append(epro::format(L"*{}\n", gDataManager->GetSysString(1631 + i)));
					}
				str.append(epro::format(L"*{}\n", gDataManager->GetSysString(1630)));
			}
		} else if (rule != DEFAULT_DUEL_RULE) {
			str.append(epro::format(L"*{}\n", gDataManager->GetSysString(1260 + rule - 1)));
		}
		if(params & DUEL_TCG_SEGOC_NONPUBLIC && params != DUEL_MODE_GOAT)
			strR.append(epro::format(L"*{}\n", gDataManager->GetSysString(1631 + (TCG_SEGOC_NONPUBLIC - CHECKBOX_OBSOLETE))));
		if(!mainGame->dInfo.compat_mode) {
			for(int flag = SEALED_DUEL, i = 0; flag < ACTION_DUEL + 1; flag = flag << 1, i++)
				if(pkt.info.extra_rules & flag) {
					strR.append(epro::format(L"*{}\n", gDataManager->GetSysString(1132 + i)));
				}
		}
		if(pkt.info.no_check_deck_content) {
			str.append(epro::format(L"*{}\n", gDataManager->GetSysString(1229)));
		}
		if(pkt.info.no_shuffle_deck) {
			str.append(epro::format(L"*{}\n", gDataManager->GetSysString(1230)));
		}
		static constexpr DeckSizes ocg_deck_sizes{ {40,60}, {0,15}, {0,15} };
		static constexpr DeckSizes rush_deck_sizes{ {40,60}, {0,15}, {0,15} };
		static constexpr DeckSizes speed_deck_sizes{ {20,30}, {0,6}, {0,6} };
		static constexpr DeckSizes goat_deck_sizes{ {40,60}, {0,999}, {0,15} };
		static constexpr DeckSizes empty_deck_sizes{ {0,0}, {0,0}, {0,0} }; // compat mode
		if(pkt.info.sizes != empty_deck_sizes) {
			do {
				if(rule < 6) {
					if(pkt.info.sizes == ocg_deck_sizes)
						break;
				} else {
					if(params == DUEL_MODE_RUSH && pkt.info.sizes == rush_deck_sizes)
						break;
					if(params == DUEL_MODE_GOAT && pkt.info.sizes == goat_deck_sizes)
						break;
					if(params == DUEL_MODE_SPEED && pkt.info.sizes == speed_deck_sizes)
						break;
				}
				str.append(epro::format(L"*{}\n", gDataManager->GetSysString(12112)));
			} while(0);
		}
		static constexpr std::pair<uint32_t, uint32_t> MONSTER_TYPES[]{
			{ TYPE_FUSION, 1056 },
			{ TYPE_SYNCHRO, 1063 },
			{ TYPE_XYZ, 1073 },
			{ TYPE_PENDULUM, 1074 },
			{ TYPE_LINK, 1076 }
		};
		for (const auto& pair : MONSTER_TYPES) {
			if (pkt.info.forbiddentypes & pair.first) {
				strL += epro::sprintf(gDataManager->GetSysString(1627), gDataManager->GetSysString(pair.second));
				strL += L"\n";
			}
		}
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		matManager.SetActiveVertices(mainGame->dInfo.HasFieldFlag(DUEL_3_COLUMNS_FIELD),
									 !mainGame->dInfo.HasFieldFlag(DUEL_SEPARATE_PZONE));
		int x = (pkt.info.team1 + pkt.info.team2 >= 5) ? 60 : 0;
		mainGame->btnHostPrepOB->setRelativePosition(mainGame->Scale<irr::s32>(10, 180 + x, 110, 205 + x));
		mainGame->stHostPrepOB->setRelativePosition(mainGame->Scale<irr::s32>(10, 210 + x, 270, 230 + x));
		mainGame->stHostPrepRule->setRelativePosition(mainGame->Scale<irr::s32>(280, 30, 460, 270 + x));
		mainGame->stDeckSelect->setRelativePosition(mainGame->Scale<irr::s32>(10, 235 + x, 110, 255 + x));
		mainGame->cbDeckSelect->setRelativePosition(mainGame->Scale<irr::s32>(120, 230 + x, 270, 255 + x));
		mainGame->btnHostPrepReady->setRelativePosition(mainGame->Scale<irr::s32>(170, 180 + x, 270, 205 + x));
		mainGame->btnHostPrepNotReady->setRelativePosition(mainGame->Scale<irr::s32>(170, 180 + x, 270, 205 + x));
		mainGame->btnHostPrepStart->setRelativePosition(mainGame->Scale<irr::s32>(230, 280 + x, 340, 305 + x));
		mainGame->btnHostPrepCancel->setRelativePosition(mainGame->Scale<irr::s32>(350, 280 + x, 460, 305 + x));
		mainGame->wHostPrepare->setRelativePosition(mainGame->ResizeWin(270, 120, 750, 440 + x));
		mainGame->wHostPrepareR->setRelativePosition(mainGame->ResizeWin(750, 120, 950, 440 + x));
		mainGame->wHostPrepareL->setRelativePosition(mainGame->ResizeWin(70, 120, 270, 440 + x));
		mainGame->gBot.window->setRelativePosition(irr::core::position2di(mainGame->wHostPrepare->getAbsolutePosition().LowerRightCorner.X, mainGame->wHostPrepare->getAbsolutePosition().UpperLeftCorner.Y));
		for(int i = 0; i < 6; i++) {
			mainGame->chkHostPrepReady[i]->setVisible(false);
			mainGame->chkHostPrepReady[i]->setChecked(false);
			mainGame->btnHostPrepKick[i]->setVisible(false);
			mainGame->stHostPrepDuelist[i]->setVisible(false);
			mainGame->stHostPrepDuelist[i]->setText(L"");
		}
		for(int i = 0; i < pkt.info.team1; i++) {
			mainGame->chkHostPrepReady[i]->setVisible(true);
			mainGame->stHostPrepDuelist[i]->setVisible(true);
			mainGame->btnHostPrepKick[i]->setRelativePosition(mainGame->Scale<irr::s32>(10, 65 + i * 25, 30, 85 + i * 25));
			mainGame->stHostPrepDuelist[i]->setRelativePosition(mainGame->Scale<irr::s32>(40, 65 + i * 25, 240, 85 + i * 25));
			mainGame->chkHostPrepReady[i]->setRelativePosition(mainGame->Scale<irr::s32>(250, 65 + i * 25, 270, 85 + i * 25));
		}
		for(int i = pkt.info.team1; i < pkt.info.team1 + pkt.info.team2; i++) {
			mainGame->chkHostPrepReady[i]->setVisible(true);
			mainGame->stHostPrepDuelist[i]->setVisible(true);
			mainGame->btnHostPrepKick[i]->setRelativePosition(mainGame->Scale<irr::s32>(10, 10 + 65 + i * 25, 30, 10 + 85 + i * 25));
			mainGame->stHostPrepDuelist[i]->setRelativePosition(mainGame->Scale<irr::s32>(40, 10 + 65 + i * 25, 240, 10 + 85 + i * 25));
			mainGame->chkHostPrepReady[i]->setRelativePosition(mainGame->Scale<irr::s32>(250, 10 + 65 + i * 25, 270, 10 + 85 + i * 25));
		}
		mainGame->dInfo.selfnames.resize(pkt.info.team1);
		mainGame->dInfo.opponames.resize(pkt.info.team2);
		mainGame->btnHostPrepReady->setVisible(true);
		mainGame->btnHostPrepNotReady->setVisible(false);
		mainGame->dInfo.time_limit = pkt.info.time_limit;
		mainGame->dInfo.time_left[0] = 0;
		mainGame->dInfo.time_left[1] = 0;
		mainGame->deckBuilder.filterList = 0;
		for(auto lit = gdeckManager->_lfList.begin(); lit != gdeckManager->_lfList.end(); ++lit)
			if(lit->hash == pkt.info.lflist)
				mainGame->deckBuilder.filterList = &(*lit);
		if(mainGame->deckBuilder.filterList == 0)
			mainGame->deckBuilder.filterList = &gdeckManager->_lfList[0];
		watching = 0;
		mainGame->stHostPrepOB->setText(epro::format(L"{} {}", gDataManager->GetSysString(1253), watching).data());
		mainGame->stHostPrepRule->setText(str.data());
		mainGame->stHostPrepRuleR->setText(strR.data());
		mainGame->stHostPrepRuleL->setText(strL.data());
		mainGame->RefreshDeck(mainGame->cbDeckSelect);
		mainGame->cbDeckSelect->setEnabled(true);
		if(mainGame->wCreateHost->isVisible())
			mainGame->HideElement(mainGame->wCreateHost);
		else if (mainGame->wLanWindow->isVisible())
			mainGame->HideElement(mainGame->wLanWindow);
		mainGame->ShowElement(mainGame->wHostPrepare);
		if(strR.size())
			mainGame->ShowElement(mainGame->wHostPrepareR);
		if(strL.size())
			mainGame->ShowElement(mainGame->wHostPrepareL);
		mainGame->wChat->setVisible(true);
		mainGame->dInfo.isFirst = (mainGame->dInfo.player_type < mainGame->dInfo.team1) || (mainGame->dInfo.player_type >= 7);
		mainGame->dInfo.isTeam1 = mainGame->dInfo.isFirst;
		connect_state |= 0x4;
		break;
	}
	case STOC_TYPE_CHANGE: {
		auto pkt = BufferIO::getStruct<STOC_TypeChange>(pdata, len);
		selftype = pkt.type & 0xf;
		is_host = ((pkt.type >> 4) & 0xf) != 0;
		for(int i = 0; i < mainGame->dInfo.team1 + mainGame->dInfo.team2; i++) {
			mainGame->btnHostPrepKick[i]->setVisible(is_host);
		}
		for(int i = 0; i < mainGame->dInfo.team1 + mainGame->dInfo.team2; i++) {
			mainGame->chkHostPrepReady[i]->setEnabled(false);
		}
		if(selftype >= mainGame->dInfo.team1 + mainGame->dInfo.team2) {
			mainGame->btnHostPrepDuelist->setEnabled(true);
			mainGame->btnHostPrepOB->setEnabled(false);
			mainGame->btnHostPrepReady->setVisible(false);
			mainGame->btnHostPrepNotReady->setVisible(false);
		} else {
			mainGame->chkHostPrepReady[selftype]->setEnabled(true);
			mainGame->chkHostPrepReady[selftype]->setChecked(false);
			mainGame->btnHostPrepDuelist->setEnabled(mainGame->dInfo.team1 + mainGame->dInfo.team2 > 2);
			mainGame->btnHostPrepOB->setEnabled(true);
			mainGame->btnHostPrepReady->setVisible(true);
			mainGame->btnHostPrepNotReady->setVisible(false);
		}
		mainGame->btnHostPrepWindBot->setVisible(is_host && !mainGame->isHostingOnline);
		mainGame->btnHostPrepStart->setVisible(is_host);
		mainGame->btnHostPrepStart->setEnabled(is_host && CheckReady());
		mainGame->dInfo.player_type = selftype;
		mainGame->dInfo.isFirst = (mainGame->dInfo.player_type < mainGame->dInfo.team1) || (mainGame->dInfo.player_type >= 7);
		mainGame->dInfo.isTeam1 = mainGame->dInfo.isFirst;
		break;
	}
	case STOC_DUEL_START: {
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->HideElement(mainGame->wHostPrepare);
		mainGame->HideElement(mainGame->gBot.window);
		mainGame->HideElement(mainGame->wHostPrepareL);
		mainGame->HideElement(mainGame->wHostPrepareR);
		mainGame->WaitFrameSignal(11, lock);
		mainGame->dField.Clear();
		mainGame->dInfo.isInLobby = false;
		mainGame->is_siding = false;
		mainGame->dInfo.checkRematch = false;
		mainGame->dInfo.isInDuel = true;
		mainGame->dInfo.isStarted = false;
		mainGame->dInfo.lp[0] = 0;
		mainGame->dInfo.lp[1] = 0;
		mainGame->dInfo.turn = 0;
		mainGame->dInfo.time_left[0] = 0;
		mainGame->dInfo.time_left[1] = 0;
		mainGame->dInfo.time_player = 2;
		mainGame->dInfo.current_player[0] = 0;
		mainGame->dInfo.current_player[1] = 0;
		mainGame->dInfo.isReplaySwapped = false;
		mainGame->is_building = false;
		mainGame->mTopMenu->setVisible(false);
		mainGame->wCardImg->setVisible(true);
		mainGame->wInfos->setVisible(true);
		mainGame->wPhase->setVisible(true);
		mainGame->btnSideOK->setVisible(false);
		mainGame->btnDP->setVisible(false);
		mainGame->btnDP->setSubElement(false);
		mainGame->btnSP->setVisible(false);
		mainGame->btnSP->setSubElement(false);
		mainGame->btnM1->setVisible(false);
		mainGame->btnM1->setSubElement(false);
		mainGame->btnBP->setVisible(false);
		mainGame->btnBP->setSubElement(false);
		mainGame->btnM2->setVisible(false);
		mainGame->btnM2->setSubElement(false);
		mainGame->btnEP->setVisible(false);
		mainGame->btnEP->setSubElement(false);
		mainGame->btnShuffle->setVisible(false);
		mainGame->btnSideShuffle->setVisible(false);
		mainGame->btnSideSort->setVisible(false);
		mainGame->btnSideReload->setVisible(false);
		mainGame->wChat->setVisible(true);
		mainGame->device->setEventReceiver(&mainGame->dField);
		mainGame->SetPhaseButtons();
		mainGame->SetMessageWindow();
		mainGame->dInfo.selfnames.clear();
		mainGame->dInfo.opponames.clear();
		int i;
		for(i = 0; i < mainGame->dInfo.team1; i++) {
			mainGame->dInfo.selfnames.push_back(mainGame->stHostPrepDuelist[i]->getText());
		}
		for(; i < mainGame->dInfo.team1 + mainGame->dInfo.team2; i++) {
			mainGame->dInfo.opponames.push_back(mainGame->stHostPrepDuelist[i]->getText());
		}
		if(selftype >= mainGame->dInfo.team1 + mainGame->dInfo.team2) {
			mainGame->dInfo.player_type = 7;
			mainGame->btnLeaveGame->setText(gDataManager->GetSysString(1350).data());
			mainGame->btnLeaveGame->setVisible(true);
			mainGame->btnSpectatorSwap->setVisible(true);
			mainGame->dInfo.isFirst = true;
			mainGame->dInfo.isTeam1 = true;
		} else {
			mainGame->dInfo.isFirst = selftype < mainGame->dInfo.team1;
			mainGame->dInfo.isTeam1 = mainGame->dInfo.isFirst;
		}
		mainGame->dInfo.current_player[0] = 0;
		mainGame->dInfo.current_player[1] = 0;
		match_kill = 0;
		replay_stream.clear();
		break;
	}
	case STOC_DUEL_END: {
		gSoundManager->StopSounds();
		{
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			if(mainGame->dInfo.player_type < 7)
				mainGame->btnLeaveGame->setVisible(false);
			mainGame->btnSpectatorSwap->setVisible(false);
			mainGame->btnChainIgnore->setVisible(false);
			mainGame->btnChainAlways->setVisible(false);
			mainGame->btnChainWhenAvail->setVisible(false);
			mainGame->stMessage->setText(gDataManager->GetSysString(1500).data());
			mainGame->btnCancelOrFinish->setVisible(false);
			if(mainGame->wQuery->isVisible())
				mainGame->HideElement(mainGame->wQuery);
			if(mainGame->wPosSelect->isVisible())
				mainGame->HideElement(mainGame->wPosSelect);
			if(mainGame->wCardSelect->isVisible())
				mainGame->HideElement(mainGame->wCardSelect);
			if(mainGame->wCardDisplay->isVisible())
				mainGame->HideElement(mainGame->wCardDisplay);
			if(mainGame->wANNumber->isVisible())
				mainGame->HideElement(mainGame->wANNumber);
			if(mainGame->wANCard->isVisible())
				mainGame->HideElement(mainGame->wANCard);
			mainGame->PopupElement(mainGame->wMessage);
			mainGame->actionSignal.Wait(lock);
			mainGame->closeDuelWindow = true;
			mainGame->closeDoneSignal.Wait(lock);
			mainGame->dInfo.isInLobby = false;
			mainGame->dInfo.checkRematch = false;
			mainGame->dInfo.isInDuel = false;
			mainGame->dInfo.isStarted = false;
			mainGame->dField.Clear();
			mainGame->is_building = false;
			mainGame->is_siding = false;
			mainGame->wDeckEdit->setVisible(false);
			mainGame->btnCreateHost->setEnabled(mainGame->coreloaded);
			mainGame->btnJoinHost->setEnabled(true);
			mainGame->btnJoinCancel->setEnabled(true);
			mainGame->stTip->setVisible(false);
			mainGame->device->setEventReceiver(&mainGame->menuHandler);
			if(mainGame->isHostingOnline) {
				mainGame->ShowElement(mainGame->wRoomListPlaceholder);
			} else {
				mainGame->ShowElement(mainGame->wLanWindow);
			}
			mainGame->SetMessageWindow();
		}
		connect_state |= 0x100;
		event_base_loopbreak(client_base);
		break;
	}
	case STOC_REPLAY: {
		ReplayPrompt(mainGame->dInfo.compat_mode || last_replay.pheader.base.id == 0);
		break;
	}
	case STOC_TIME_LIMIT: {
		auto pkt = BufferIO::getStruct<STOC_TimeLimit>(pdata, len);
		int lplayer = mainGame->LocalPlayer(pkt.player);
		if(lplayer == 0)
			DuelClient::SendPacketToServer(CTOS_TIME_CONFIRM);
		mainGame->dInfo.time_player = lplayer;
		mainGame->dInfo.time_left[lplayer] = pkt.left_time;
		break;
	}
	case STOC_CHAT:	{
		auto pkt = BufferIO::getStruct<STOC_Chat>(pdata, data.size());
		int player = pkt.player;
		int type = -1;
		if(player < mainGame->dInfo.team1 + mainGame->dInfo.team2) {
			int team1 = mainGame->dInfo.team1;
			int team2 = mainGame->dInfo.team2;
			if((mainGame->dInfo.isTeam1 && !mainGame->dInfo.isFirst) ||
			   (!mainGame->dInfo.isTeam1 && mainGame->dInfo.isFirst)) {
				std::swap(team1, team2);
			}
			if(player >= team1) {
				player -= team1;
				type = 1;
			} else {
				type = 0;
			}
			if((!mainGame->dInfo.isFirst && mainGame->dInfo.isTeam1) ||
			   (mainGame->dInfo.isFirst && !mainGame->dInfo.isTeam1))
				type = 1 - type;
			if(((type == 1 && mainGame->dInfo.isTeam1) || (type == 0 && !mainGame->dInfo.isTeam1)) && mainGame->tabSettings.chkIgnoreOpponents->isChecked())
				return;
		} else {
			type = 2;
			if(player == 8) { //system custom message.
				if(mainGame->tabSettings.chkIgnoreOpponents->isChecked())
					return;
			} else if(player < 11 || player > 19) {
				if(mainGame->tabSettings.chkIgnoreSpectators->isChecked())
					return;
				player = 10;
			}
		}
		wchar_t msg[256];
		BufferIO::DecodeUTF16(pkt.msg, msg, 256);
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->AddChatMsg(msg, player, type);
		break;
	}
	case STOC_HS_PLAYER_ENTER: {
		gSoundManager->PlaySoundEffect(SoundManager::SFX::PLAYER_ENTER);
		auto pkt = BufferIO::getStruct<STOC_HS_PlayerEnter>(pdata, len);
		if(pkt.pos > 5)
			break;
		wchar_t name[20];
		BufferIO::DecodeUTF16(pkt.name, name, 20);
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		if(pkt.pos < mainGame->dInfo.team1)
			mainGame->dInfo.selfnames[pkt.pos] = name;
		else
			mainGame->dInfo.opponames[pkt.pos - mainGame->dInfo.team1] = name;
		mainGame->stHostPrepDuelist[pkt.pos]->setText(name);
		mainGame->btnHostPrepStart->setVisible(is_host);
		mainGame->btnHostPrepStart->setEnabled(is_host && CheckReady());
		break;
	}
	case STOC_HS_PLAYER_CHANGE: {
		auto pkt = BufferIO::getStruct<STOC_HS_PlayerChange>(pdata, len);
		uint8_t pos = (pkt.status >> 4) & 0xf;
		uint8_t state = pkt.status & 0xf;
		if(pos > 5)
			break;
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		if(state < 8) {
			gSoundManager->PlaySoundEffect(SoundManager::SFX::PLAYER_ENTER);
			std::wstring prename = mainGame->stHostPrepDuelist[pos]->getText();
			mainGame->stHostPrepDuelist[state]->setText(prename.data());
			mainGame->stHostPrepDuelist[pos]->setText(L"");
			mainGame->chkHostPrepReady[pos]->setChecked(false);
			if(pos < mainGame->dInfo.team1)
				mainGame->dInfo.selfnames[pos] = L"";
			else
				mainGame->dInfo.opponames[pos - mainGame->dInfo.team1] = L"";
			if(state < mainGame->dInfo.team1)
				mainGame->dInfo.selfnames[state] = prename;
			else
				mainGame->dInfo.opponames[state - mainGame->dInfo.team1] = prename;
		} else if(state == PLAYERCHANGE_READY) {
			mainGame->chkHostPrepReady[pos]->setChecked(true);
			if(pos == selftype) {
				mainGame->btnHostPrepReady->setVisible(false);
				mainGame->btnHostPrepNotReady->setVisible(true);
			}
		} else if(state == PLAYERCHANGE_NOTREADY) {
			mainGame->chkHostPrepReady[pos]->setChecked(false);
			if(pos == selftype) {
				mainGame->btnHostPrepReady->setVisible(true);
				mainGame->btnHostPrepNotReady->setVisible(false);
			}
		} else if(state == PLAYERCHANGE_LEAVE) {
			mainGame->stHostPrepDuelist[pos]->setText(L"");
			mainGame->chkHostPrepReady[pos]->setChecked(false);
		} else if(state == PLAYERCHANGE_OBSERVE) {
			watching++;
			mainGame->stHostPrepDuelist[pos]->setText(L"");
			mainGame->chkHostPrepReady[pos]->setChecked(false);
			mainGame->stHostPrepOB->setText(epro::format(L"{} {}", gDataManager->GetSysString(1253), watching).data());
		}
		mainGame->btnHostPrepStart->setVisible(is_host);
		mainGame->btnHostPrepStart->setEnabled(is_host && CheckReady());
		break;
	}
	case STOC_HS_WATCH_CHANGE: {
		auto pkt = BufferIO::getStruct<STOC_HS_WatchChange>(pdata, len);
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		watching = pkt.watch_count;
		mainGame->stHostPrepOB->setText(epro::format(L"{} {}", gDataManager->GetSysString(1253), watching).data());
		break;
	}
	case STOC_NEW_REPLAY: {
		last_replay.pheader.base.id = 0;
		uint32_t replay_header_size;
		if(ExtendedReplayHeader::ParseReplayHeader(pdata, len, last_replay.pheader, &replay_header_size)) {
			const auto total_data_size = len - (replay_header_size - sizeof(uint8_t));
			last_replay.comp_data.resize(total_data_size);
			memcpy(last_replay.comp_data.data(), pdata + replay_header_size, total_data_size);
		}
		break;
	}
	case STOC_CATCHUP: {
		mainGame->dInfo.isCatchingUp = !!BufferIO::Read<uint8_t>(pdata);
		if(!mainGame->dInfo.isCatchingUp) {
			std::lock_guard<epro::mutex> lock(mainGame->gMutex);
			mainGame->dField.RefreshAllCards();
		}
		break;
	}
	case STOC_REMATCH: {
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->dInfo.checkRematch = true;
		if(mainGame->wQuery->isVisible())
			mainGame->HideElement(mainGame->wQuery);
		if(mainGame->wPosSelect->isVisible())
			mainGame->HideElement(mainGame->wPosSelect);
		if(mainGame->wCardSelect->isVisible())
			mainGame->HideElement(mainGame->wCardSelect);
		if(mainGame->wCardDisplay->isVisible())
			mainGame->HideElement(mainGame->wCardDisplay);
		if(mainGame->wANNumber->isVisible())
			mainGame->HideElement(mainGame->wANNumber);
		if(mainGame->wANCard->isVisible())
			mainGame->HideElement(mainGame->wANCard);
		mainGame->stQMessage->setText(gDataManager->GetSysString(1989).data());
		mainGame->PopupElement(mainGame->wQuery);
		break;
	}
	}
}
bool DuelClient::CheckReady() {
	bool ready1 = false, ready2 = false;
	for(int i = 0; i < mainGame->dInfo.team1; i++) {
		if(mainGame->stHostPrepDuelist[i]->getText()[0]) {
			ready1 = mainGame->chkHostPrepReady[i]->isChecked();
		} else if(!mainGame->dInfo.isRelay) {
			return false;
		}
	}
	for(int i = mainGame->dInfo.team1; i < mainGame->dInfo.team1 + mainGame->dInfo.team2; i++) {
		if(mainGame->stHostPrepDuelist[i]->getText()[0]) {
			ready2 = mainGame->chkHostPrepReady[i]->isChecked();
		} else if(!mainGame->dInfo.isRelay) {
			return false;
		}
	}
	return ready1 && ready2;
}
std::pair<uint32_t, uint32_t> DuelClient::GetPlayersCount() {
	uint32_t count1 = 0, count2 = 0;
	for(int i = 0; i < mainGame->dInfo.team1; i++) {
		if(mainGame->stHostPrepDuelist[i]->getText()[0]) {
			count1++;
		}
	}
	for(int i = mainGame->dInfo.team1; i < mainGame->dInfo.team1 + mainGame->dInfo.team2; i++) {
		if(mainGame->stHostPrepDuelist[i]->getText()[0]) {
			count2++;
		}
	}
	return { count1, count2 };
}
template<typename T1, typename T2>
inline T2 CompatRead(uint8_t*& buf) {
	if(mainGame->dInfo.compat_mode)
		return static_cast<T2>(BufferIO::Read<T1>(buf));
	return BufferIO::Read<T2>(buf);
}
template<typename T1, typename T2>
inline T2 CompatRead(const uint8_t*& buf) {
	if(mainGame->dInfo.compat_mode)
		return static_cast<T2>(BufferIO::Read<T1>(buf));
	return BufferIO::Read<T2>(buf);
}
inline void Play(SoundManager::SFX sound) {
	if(!mainGame->dInfo.isCatchingUp)
		gSoundManager->PlaySoundEffect(sound);
}
inline bool PlayChant(SoundManager::CHANT sound, uint32_t code) {
	if(!mainGame->dInfo.isCatchingUp)
		return gSoundManager->PlayChant(sound, code);
	return true;
}
inline std::unique_lock<epro::mutex> LockIf() {
	if(!mainGame->dInfo.isCatchingUp || !mainGame->dInfo.isReplay)
		return std::unique_lock<epro::mutex>(mainGame->gMutex);
	return std::unique_lock<epro::mutex>();
}
int DuelClient::ClientAnalyze(const uint8_t* msg, uint32_t len) {
	const auto* pbuf = msg;
	if(!mainGame->dInfo.isReplay && !mainGame->dInfo.isSingleMode) {
		mainGame->dInfo.curMsg = BufferIO::Read<uint8_t>(pbuf);
		len--;
		if(mainGame->dInfo.curMsg != MSG_WAITING) {
			replay_stream.emplace_back(mainGame->dInfo.curMsg, pbuf, len);
		}
	}
	mainGame->wCmdMenu->setVisible(false);
	if(!mainGame->dInfo.isReplay && mainGame->dInfo.curMsg != MSG_WAITING) {
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->waitFrame = -1;
		mainGame->stHintMsg->setVisible(false);
		if(mainGame->wCardSelect->isVisible()) {
			mainGame->HideElement(mainGame->wCardSelect);
			mainGame->WaitFrameSignal(11, lock);
		}
		/*if(mainGame->wCardDisplay->isVisible()) {
			mainGame->HideElement(mainGame->wCardDisplay);
			mainGame->WaitFrameSignal(11, lock);
		}*/
		if(mainGame->wOptions->isVisible()) {
			mainGame->HideElement(mainGame->wOptions);
			mainGame->WaitFrameSignal(11, lock);
		}
	}
	if(mainGame->dInfo.time_player == 1)
		mainGame->dInfo.time_player = 2;
	if(is_swapping) {
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->dField.ReplaySwap();
		is_swapping = false;
	}
	switch(mainGame->dInfo.curMsg) {
	case MSG_RETRY: {
		if(!mainGame->dInfo.compat_mode) {
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->stMessage->setText(gDataManager->GetSysString(1434).data());
			mainGame->PopupElement(mainGame->wMessage);
			mainGame->actionSignal.Wait(lock);
			return true;
		} else {
			gSoundManager->StopSounds();
			{
				std::unique_lock<epro::mutex> lock(mainGame->gMutex);
				mainGame->stMessage->setText(gDataManager->GetSysString(1434).data());
				mainGame->PopupElement(mainGame->wMessage);
				mainGame->actionSignal.Wait(lock);
				mainGame->closeDuelWindow = true;
				mainGame->closeDoneSignal.Wait(lock);
				lock.unlock();
				ReplayPrompt(true);
				lock.lock();
				mainGame->dField.Clear();
				mainGame->dInfo.isInLobby = false;
				mainGame->dInfo.isInDuel = false;
				mainGame->dInfo.checkRematch = false;
				mainGame->dInfo.isStarted = false;
				mainGame->btnCreateHost->setEnabled(mainGame->coreloaded);
				mainGame->btnJoinHost->setEnabled(true);
				mainGame->btnJoinCancel->setEnabled(true);
				mainGame->stTip->setVisible(false);
				gSoundManager->StopSounds();
				mainGame->device->setEventReceiver(&mainGame->menuHandler);
				if(mainGame->isHostingOnline) {
					mainGame->ShowElement(mainGame->wRoomListPlaceholder);
				} else {
					mainGame->ShowElement(mainGame->wLanWindow);
				}
				mainGame->SetMessageWindow();
			}
			connect_state |= 0x100;
			event_base_loopbreak(client_base);
			return false;
		}
	}
	case MSG_HINT: {
		const auto type = BufferIO::Read<uint8_t>(pbuf);
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		uint64_t data = CompatRead<uint32_t, uint64_t>(pbuf);
		if(mainGame->dInfo.isCatchingUp && type < HINT_SKILL)
			return true;
		if(mainGame->dInfo.isReplay && (type == 1 || type == 2 || type == 3 || type == 5))
			return true;
		switch (type) {
		case HINT_EVENT: {
			event_string = gDataManager->GetDesc(data, mainGame->dInfo.compat_mode).data();
			break;
		}
		case HINT_MESSAGE: {
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->stMessage->setText(gDataManager->GetDesc(data, mainGame->dInfo.compat_mode).data());
			mainGame->PopupElement(mainGame->wMessage);
			mainGame->actionSignal.Wait(lock);
			break;
		}
		case HINT_SELECTMSG: {
			select_hint = data;
			break;
		}
		case HINT_OPSELECTED: {
			std::wstring text(epro::format(gDataManager->GetSysString(player == 0 ? 1510 : 1512), gDataManager->GetDesc(data, mainGame->dInfo.compat_mode)));
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->AddLog(text);
			mainGame->stACMessage->setText(text.data());
			mainGame->PopupElement(mainGame->wACMessage, 20);
			mainGame->WaitFrameSignal(40, lock);
			break;
		}
		case HINT_EFFECT: {
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->showcardcode = data;
			mainGame->showcarddif = 0;
			mainGame->showcard = 1;
			mainGame->WaitFrameSignal(30, lock);
			break;
		}
		case HINT_RACE: {
			std::wstring text(epro::format(gDataManager->GetSysString(1511), gDataManager->FormatRace(data)));
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->AddLog(text);
			mainGame->stACMessage->setText(text.data());
			mainGame->PopupElement(mainGame->wACMessage, 20);
			mainGame->WaitFrameSignal(40, lock);
			break;
		}
		case HINT_ATTRIB: {
			std::wstring text(epro::format(gDataManager->GetSysString(1511), gDataManager->FormatAttribute(data)));
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->AddLog(text);
			mainGame->stACMessage->setText(text.data());
			mainGame->PopupElement(mainGame->wACMessage, 20);
			mainGame->WaitFrameSignal(40, lock);
			break;
		}
		case HINT_CODE: {
			std::wstring text(epro::format(gDataManager->GetSysString(1511), gDataManager->GetName(data)));
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->AddLog(text);
			mainGame->stACMessage->setText(text.data());
			mainGame->PopupElement(mainGame->wACMessage, 20);
			mainGame->WaitFrameSignal(40, lock);
			break;
		}
		case HINT_NUMBER: {
			std::wstring text(epro::format(gDataManager->GetSysString(1512), data));
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->AddLog(text);
			mainGame->stACMessage->setText(text.data());
			mainGame->PopupElement(mainGame->wACMessage, 20);
			mainGame->WaitFrameSignal(40, lock);
			break;
		}
		case HINT_CARD: {
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->showcardcode = data;
			mainGame->showcarddif = 0;
			mainGame->showcard = 1;
			Play(SoundManager::SFX::ACTIVATE);
			mainGame->WaitFrameSignal(30, lock);
			break;
		}
		case HINT_ZONE: {
			if(player == 1)
				data = (data >> 16) | (data << 16);
			std::vector<std::wstring> tmp;
			for(uint32_t filter = 0x1; filter != 0; filter <<= 1) {
				if(uint32_t s = filter & data) {
					uint32_t player_string{};
					uint32_t zone_string{};
					uint32_t seq = 1;
					if(s & 0x60) {
						player_string = 1081;
						data &= ~0x600000;
					} else if(s & 0xffff) {
						player_string = 102;
					} else if(s & 0xffff0000) {
						player_string = 103;
						s >>= 16;
					}
					if(s & 0x1f) {
						zone_string = 1002;
					} else if(s & 0xff00) {
						s >>= 8;
						if(s & 0x1f)
							zone_string = 1003;
						else if(s & 0x20)
							zone_string = 1008;
						else if(s & 0xc0)
							zone_string = 1009;
					}
					for(int i = 0x1; i < 0x100; i <<= 1) {
						if(s & i)
							break;
						++seq;
					}
					const auto tmp_string = epro::format(L"{}{}({})", gDataManager->GetSysString(player_string), gDataManager->GetSysString(zone_string), seq);
					tmp.push_back(epro::format(gDataManager->GetSysString(1512), tmp_string));
				}
			}
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			for(const auto& str : tmp)
				mainGame->AddLog(str);
			mainGame->dField.selectable_field = data;
			mainGame->WaitFrameSignal(40, lock);
			mainGame->dField.selectable_field = 0;
			break;
		}
		case HINT_SKILL: {
			auto lock = LockIf();
			auto& pcard = mainGame->dField.skills[player];
			if(!pcard) {
				pcard = new ClientCard{};
				pcard->controler = player;
				pcard->sequence = 0;
				pcard->position = POS_FACEUP;
				pcard->location = LOCATION_SKILL;
			}
			pcard->SetCode(data & 0xffffffff);
			if(!mainGame->dInfo.isCatchingUp) {
				mainGame->dField.MoveCard(pcard, 10);
				mainGame->WaitFrameSignal(11, lock);
			}
			break;
		}
		case HINT_SKILL_COVER: {
			auto lock = LockIf();
			auto& pcard = mainGame->dField.skills[player];
			if(!pcard) {
				pcard = new ClientCard{};
				pcard->controler = player;
				pcard->sequence = 0;
				pcard->position = POS_FACEDOWN;
				pcard->location = LOCATION_SKILL;
			}
			pcard->cover = data & 0xffffffff;
			pcard->SetCode((data >> 32) & 0xffffffff);
			if(!mainGame->dInfo.isCatchingUp) {
				mainGame->dField.MoveCard(pcard, 10);
				mainGame->WaitFrameSignal(11, lock);
			}
			break;
		}
		case HINT_SKILL_FLIP: {
			auto lock = LockIf();
			auto& pcard = mainGame->dField.skills[player];
			if(!pcard) {
				pcard = new ClientCard{};
				pcard->controler = player;
				pcard->sequence = 0;
				pcard->position = POS_FACEDOWN;
				pcard->location = LOCATION_SKILL;
			}
			pcard->SetCode(data & 0xffffffff);
			if(data & 0x100000000) {
				pcard->position = POS_FACEUP;
			} else if(data & 0x200000000) {
				pcard->position = POS_FACEDOWN;
			}
			if(!mainGame->dInfo.isCatchingUp) {
				mainGame->dField.MoveCard(pcard, 10);
				mainGame->WaitFrameSignal(11, lock);
			}
			break;
		}
		case HINT_SKILL_REMOVE: {
			auto lock = LockIf();
			auto& pcard = mainGame->dField.skills[player];
			if(pcard) {
				if(!mainGame->dInfo.isCatchingUp) {
					int frames = 20;
					if(gGameConfig->quick_animation)
						frames = 12;
					mainGame->dField.FadeCard(pcard, 5, frames);
					mainGame->WaitFrameSignal(frames, lock);
				}
				if(pcard == mainGame->dField.hovered_card)
					mainGame->dField.hovered_card = nullptr;
				delete pcard;
				pcard = nullptr;
			}
			break;
		}
		}
		break;
	}
	case MSG_WIN: {
		uint8_t player = BufferIO::Read<uint8_t>(pbuf);
		uint8_t type = BufferIO::Read<uint8_t>(pbuf);
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->showcarddif = 110;
		mainGame->showcardp = 0;
		mainGame->dInfo.vic_string = L"";
		mainGame->showcardcode = 3;
		std::wstring formatted_string = L"";
		if(player < 2) {
			player = mainGame->LocalPlayer(player);
			mainGame->showcardcode = player + 1;
			if(match_kill)
				mainGame->dInfo.vic_string = epro::sprintf(gDataManager->GetVictoryString(0x20), gDataManager->GetName(match_kill));
			else if(type < 0x10) {
				auto curplayer = mainGame->dInfo.current_player[1 - player];
				auto& self = mainGame->dInfo.isTeam1 ? mainGame->dInfo.selfnames : mainGame->dInfo.opponames;
				auto& oppo = mainGame->dInfo.isTeam1 ? mainGame->dInfo.opponames : mainGame->dInfo.selfnames;
				auto& names = (player == 0) ? oppo : self;
				mainGame->dInfo.vic_string = epro::format(L"[{}] {}", names[curplayer], gDataManager->GetVictoryString(type));
			} else
				mainGame->dInfo.vic_string = gDataManager->GetVictoryString(type).data();
		}
		mainGame->showcard = 101;
		mainGame->WaitFrameSignal(120, lock);
		mainGame->dInfo.vic_string = L"";
		mainGame->showcard = 0;
		break;
	}
	case MSG_WAITING: {
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->waitFrame = 0;
		mainGame->stHintMsg->setText(gDataManager->GetSysString(1390).data());
		mainGame->stHintMsg->setVisible(true);
		return true;
	}
	case MSG_START: {
		const auto playertype = BufferIO::Read<uint8_t>(pbuf);
		if(mainGame->dInfo.compat_mode)
			/*duel_rule = */BufferIO::Read<uint8_t>(pbuf);
		auto lock = LockIf();
		mainGame->wPhase->setVisible(true);
		if(!mainGame->dInfo.isCatchingUp) {
			mainGame->showcardcode = 11;
			mainGame->showcarddif = 30;
			mainGame->showcardp = 0;
			mainGame->showcard = 101;
			mainGame->WaitFrameSignal(40, lock);
			mainGame->showcard = 0;
		}
		mainGame->dInfo.isStarted = true;
		mainGame->dInfo.isFirst = (playertype & 0xf) ? false : true;
		if(playertype & 0xf0)
			mainGame->dInfo.player_type = 7;
		if(!mainGame->dInfo.isRelay) {
			if(mainGame->dInfo.isFirst) {
				if(mainGame->dInfo.isTeam1)
					mainGame->dInfo.current_player[1] = mainGame->dInfo.team2 - 1;
				else
					mainGame->dInfo.current_player[1] = mainGame->dInfo.team1 - 1;
			} else {
				if(mainGame->dInfo.isTeam1)
					mainGame->dInfo.current_player[0] = mainGame->dInfo.team1 - 1;
				else
					mainGame->dInfo.current_player[0] = mainGame->dInfo.team2 - 1;
			}
		}
		mainGame->dInfo.lp[mainGame->LocalPlayer(0)] = BufferIO::Read<uint32_t>(pbuf);
		mainGame->dInfo.lp[mainGame->LocalPlayer(1)] = BufferIO::Read<uint32_t>(pbuf);
		if(mainGame->dInfo.lp[mainGame->LocalPlayer(0)] > 0)
			mainGame->dInfo.startlp = mainGame->dInfo.lp[mainGame->LocalPlayer(0)];
		else
			mainGame->dInfo.startlp = 8000;
		mainGame->dInfo.strLP[0] = fmt::to_wstring(mainGame->dInfo.lp[0]);
		mainGame->dInfo.strLP[1] = fmt::to_wstring(mainGame->dInfo.lp[1]);
		uint16_t deckc = BufferIO::Read<uint16_t>(pbuf);
		uint16_t extrac = BufferIO::Read<uint16_t>(pbuf);
		mainGame->dField.Initial(mainGame->LocalPlayer(0), deckc, extrac);
		deckc = BufferIO::Read<uint16_t>(pbuf);
		extrac = BufferIO::Read<uint16_t>(pbuf);
		mainGame->dField.Initial(mainGame->LocalPlayer(1), deckc, extrac);
		mainGame->dInfo.turn = 0;
		mainGame->dInfo.is_shuffling = false;
		return true;
	}
	case MSG_UPDATE_DATA: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto location = BufferIO::Read<uint8_t>(pbuf);
		auto lock = LockIf();
		mainGame->dField.UpdateFieldCard(player, location, pbuf, len - 2);
		return true;
	}
	case MSG_UPDATE_CARD: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto loc = BufferIO::Read<uint8_t>(pbuf);
		const auto seq = BufferIO::Read<uint8_t>(pbuf);
		auto lock = LockIf();
		mainGame->dField.UpdateCard(player, loc, seq, pbuf, len - 3);
		break;
	}
	case MSG_SELECT_BATTLECMD: {
		/*uint8_t selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		uint64_t desc;
		uint32_t code, count, seq;
		uint8_t con, loc/*, diratt*/;
		ClientCard* pcard;
		mainGame->dField.activatable_cards.clear();
		mainGame->dField.activatable_descs.clear();
		mainGame->dField.conti_cards.clear();
		//cards with effects that can be activated, can be an arbitrary size
		count = CompatRead<uint8_t, uint32_t>(pbuf);
		for(uint32_t i = 0; i < count; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			con = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			loc =  BufferIO::Read<uint8_t>(pbuf);
			seq = CompatRead<uint8_t, uint32_t>(pbuf);
			desc = CompatRead<uint32_t, uint64_t>(pbuf);
			pcard = mainGame->dField.GetCard(con, loc, seq);
			uint8_t flag = EFFECT_CLIENT_MODE_NORMAL;
			if(!mainGame->dInfo.compat_mode) {
				flag = BufferIO::Read<uint8_t>(pbuf);
			} else if(code & 0x80000000) {
				flag = EFFECT_CLIENT_MODE_RESOLVE;
				code &= 0x7fffffff;
			}
			if(!pcard) {
				pcard = new ClientCard{};
				pcard->code = code;
				pcard->controler = con;
				pcard->sequence = static_cast<uint32_t>(mainGame->dField.limbo_temp.size());
				mainGame->dField.limbo_temp.push_back(pcard);
			}
			mainGame->dField.activatable_cards.push_back(pcard);
			mainGame->dField.activatable_descs.push_back(std::make_pair(desc, flag));
			if(flag == EFFECT_CLIENT_MODE_RESOLVE) {
				pcard->chain_code = code;
				mainGame->dField.conti_cards.push_back(pcard);
				mainGame->dField.conti_act = true;
			} else {
				pcard->cmdFlag |= COMMAND_ACTIVATE;
				if (pcard->location == LOCATION_GRAVE)
					mainGame->dField.grave_act[pcard->controler] = true;
				else if (pcard->location == LOCATION_REMOVED)
					mainGame->dField.remove_act[pcard->controler] = true;
				else if (pcard->location == LOCATION_EXTRA)
					mainGame->dField.extra_act[pcard->controler] = true;
			}
		}
		mainGame->dField.attackable_cards.clear();
		//cards that can attack, will remain under 255
		count = CompatRead<uint8_t, uint32_t>(pbuf);
		for(uint32_t i = 0; i < count; ++i) {
			/*code = */BufferIO::Read<uint32_t>(pbuf);
			con = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			loc = BufferIO::Read<uint8_t>(pbuf);
			seq = BufferIO::Read<uint8_t>(pbuf);
			/*diratt = */BufferIO::Read<uint8_t>(pbuf);
			pcard = mainGame->dField.GetCard(con, loc, seq);
			mainGame->dField.attackable_cards.push_back(pcard);
			pcard->cmdFlag |= COMMAND_ATTACK;
		}
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		if(BufferIO::Read<uint8_t>(pbuf)) {
			mainGame->btnM2->setVisible(true);
			mainGame->btnM2->setSubElement(true);
			mainGame->btnM2->setEnabled(true);
			mainGame->btnM2->setPressed(false);
		}
		if(BufferIO::Read<uint8_t>(pbuf)) {
			mainGame->btnEP->setVisible(true);
			mainGame->btnEP->setSubElement(true);
			mainGame->btnEP->setEnabled(true);
			mainGame->btnEP->setPressed(false);
		}
		return false;
	}
	case MSG_SELECT_IDLECMD: {
		/*uint8_t selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		uint32_t code, count, seq;
		uint8_t con, loc;
		uint64_t desc;
		ClientCard* pcard;
		mainGame->dField.summonable_cards.clear();
		//cards that can be normal summoned, can be an arbitrary size
		count = CompatRead<uint8_t, uint32_t>(pbuf);
		for(uint32_t i = 0; i < count; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			con = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			loc = BufferIO::Read<uint8_t>(pbuf);
			seq = CompatRead<uint8_t, uint32_t>(pbuf);
			pcard = mainGame->dField.GetCard(con, loc, seq);
			mainGame->dField.summonable_cards.push_back(pcard);
			pcard->cmdFlag |= COMMAND_SUMMON;
		}
		mainGame->dField.spsummonable_cards.clear();
		//cards that can be special summoned, can be an arbitrary size
		count = CompatRead<uint8_t, uint32_t>(pbuf);
		for(uint32_t i = 0; i < count; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			con = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			loc = BufferIO::Read<uint8_t>(pbuf);
			seq = CompatRead<uint8_t, uint32_t>(pbuf);
			pcard = mainGame->dField.GetCard(con, loc, seq);
			mainGame->dField.spsummonable_cards.push_back(pcard);
			pcard->cmdFlag |= COMMAND_SPSUMMON;
			switch(pcard->location) {
			case LOCATION_DECK:
				pcard->SetCode(code);
				mainGame->dField.deck_act[pcard->controler] = true;
				break;
			case LOCATION_GRAVE:
				mainGame->dField.grave_act[pcard->controler] = true;
				break;
			case LOCATION_REMOVED:
				mainGame->dField.remove_act[pcard->controler] = true;
				break;
			case LOCATION_EXTRA:
				mainGame->dField.extra_act[pcard->controler] = true;
				break;
			case LOCATION_SZONE: {
				if((pcard->type & TYPE_PENDULUM) && !pcard->equipTarget && pcard->sequence == mainGame->dInfo.GetPzoneIndex(0))
					mainGame->dField.pzone_act[pcard->controler] = true;
				break;
			}
			default: break;
			}
		}
		mainGame->dField.reposable_cards.clear();
		//cards whose position can be changed, will remain under 255
		count = CompatRead<uint8_t, uint32_t>(pbuf);
		for(uint32_t i = 0; i < count; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			con = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			loc = BufferIO::Read<uint8_t>(pbuf);
			seq = BufferIO::Read<uint8_t>(pbuf);
			pcard = mainGame->dField.GetCard(con, loc, seq);
			mainGame->dField.reposable_cards.push_back(pcard);
			pcard->cmdFlag |= COMMAND_REPOS;
		}
		mainGame->dField.msetable_cards.clear();
		//cards that can be set in the mzone, can be an arbitrary size
		count = CompatRead<uint8_t, uint32_t>(pbuf);
		for(uint32_t i = 0; i < count; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			con = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			loc = BufferIO::Read<uint8_t>(pbuf);
			seq = CompatRead<uint8_t, uint32_t>(pbuf);
			pcard = mainGame->dField.GetCard(con, loc, seq);
			mainGame->dField.msetable_cards.push_back(pcard);
			pcard->cmdFlag |= COMMAND_MSET;
		}
		mainGame->dField.ssetable_cards.clear();
		//cards that can be set in the szone, can be an arbitrary size
		count = CompatRead<uint8_t, uint32_t>(pbuf);
		for(uint32_t i = 0; i < count; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			con = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			loc = BufferIO::Read<uint8_t>(pbuf);
			seq = CompatRead<uint8_t, uint32_t>(pbuf);
			pcard = mainGame->dField.GetCard(con, loc, seq);
			mainGame->dField.ssetable_cards.push_back(pcard);
			pcard->cmdFlag |= COMMAND_SSET;
		}
		mainGame->dField.activatable_cards.clear();
		mainGame->dField.activatable_descs.clear();
		mainGame->dField.conti_cards.clear();
		//cards with effects that can be activated, can be an arbitrary size
		count = CompatRead<uint8_t, uint32_t>(pbuf);
		for(uint32_t i = 0; i < count; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			con = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			loc = BufferIO::Read<uint8_t>(pbuf);
			seq = CompatRead<uint8_t, uint32_t>(pbuf);
			desc = CompatRead<uint32_t, uint64_t>(pbuf);
			pcard = mainGame->dField.GetCard(con, loc, seq);
			uint8_t flag = EFFECT_CLIENT_MODE_NORMAL;
			if(!mainGame->dInfo.compat_mode) {
				flag = BufferIO::Read<uint8_t>(pbuf);
			} else if(code & 0x80000000) {
				flag = EFFECT_CLIENT_MODE_RESOLVE;
				code &= 0x7fffffff;
			}
			if(!pcard) {
				pcard = new ClientCard{};
				pcard->code = code;
				pcard->controler = con;
				pcard->sequence = static_cast<uint32_t>(mainGame->dField.limbo_temp.size());
				mainGame->dField.limbo_temp.push_back(pcard);
			}
			mainGame->dField.activatable_cards.push_back(pcard);
			mainGame->dField.activatable_descs.push_back(std::make_pair(desc, flag));
			if(flag == EFFECT_CLIENT_MODE_RESOLVE) {
				pcard->chain_code = code;
				mainGame->dField.conti_cards.push_back(pcard);
				mainGame->dField.conti_act = true;
			} else {
				pcard->cmdFlag |= COMMAND_ACTIVATE;
				if (pcard->location == LOCATION_GRAVE)
					mainGame->dField.grave_act[pcard->controler] = true;
				else if (pcard->location == LOCATION_REMOVED)
					mainGame->dField.remove_act[pcard->controler] = true;
				else if (pcard->location == LOCATION_EXTRA)
					mainGame->dField.extra_act[pcard->controler] = true;
			}
		}
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		if(BufferIO::Read<uint8_t>(pbuf)) {
			mainGame->btnBP->setVisible(true);
			mainGame->btnBP->setSubElement(true);
			mainGame->btnBP->setEnabled(true);
			mainGame->btnBP->setPressed(false);
		}
		if(BufferIO::Read<uint8_t>(pbuf)) {
			mainGame->btnEP->setVisible(true);
			mainGame->btnEP->setSubElement(true);
			mainGame->btnEP->setEnabled(true);
			mainGame->btnEP->setPressed(false);
		}
		if (BufferIO::Read<uint8_t>(pbuf)) {
			mainGame->btnShuffle->setVisible(true);
		} else {
			mainGame->btnShuffle->setVisible(false);
		}
		return false;
	}
	case MSG_SELECT_EFFECTYN: {
		/*uint8_t selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		uint32_t code = BufferIO::Read<uint32_t>(pbuf);
		CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		info.controler = mainGame->LocalPlayer(info.controler);
		uint64_t desc = CompatRead<uint32_t, uint64_t>(pbuf);
		std::wstring text;
		if(desc == 0) {
			text = epro::format(L"{}\n{}", event_string,
							   epro::sprintf(gDataManager->GetSysString(200), gDataManager->GetName(code), gDataManager->FormatLocation(info.location, info.sequence)));
		} else if(desc == 221) {
			text = epro::format(L"{}\n{}\n{}", event_string,
							   epro::sprintf(gDataManager->GetSysString(221), gDataManager->GetName(code), gDataManager->FormatLocation(info.location, info.sequence)),
							   gDataManager->GetSysString(223));
		} else {
			text = epro::sprintf(gDataManager->GetDesc(desc, mainGame->dInfo.compat_mode), gDataManager->GetName(code));
		}
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		ClientCard* pcard = mainGame->dField.GetCard(info.controler, info.location, info.sequence);
		if (pcard->code != code)
			pcard->SetCode(code);
		if(info.location != LOCATION_DECK) {
			pcard->is_highlighting = true;
			mainGame->dField.highlighting_card = pcard;
		}
		mainGame->stQMessage->setText(text.data());
		mainGame->PopupElement(mainGame->wQuery);
		return false;
	}
	case MSG_SELECT_YESNO: {
		/*uint8_t selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		uint64_t desc = CompatRead<uint32_t, uint64_t>(pbuf);
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->dField.highlighting_card = 0;
		mainGame->stQMessage->setText(gDataManager->GetDesc(desc, mainGame->dInfo.compat_mode).data());
		mainGame->PopupElement(mainGame->wQuery);
		return false;
	}
	case MSG_SELECT_OPTION: {
		/*uint8_t selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		uint8_t count = BufferIO::Read<uint8_t>(pbuf);
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->dField.select_options.clear();
		for(int i = 0; i < count; ++i)
			mainGame->dField.select_options.push_back(CompatRead<uint32_t, uint64_t>(pbuf));
		mainGame->dField.ShowSelectOption(select_hint, false);
		select_hint = 0;
		return false;
	}
	case MSG_SELECT_CARD: {
		/*uint8_t selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		mainGame->dField.select_cancelable = BufferIO::Read<uint8_t>(pbuf) != 0;
		mainGame->dField.select_min = CompatRead<uint8_t, uint32_t>(pbuf);
		mainGame->dField.select_max = CompatRead<uint8_t, uint32_t>(pbuf);
		uint32_t count = CompatRead<uint8_t, uint32_t>(pbuf);
		mainGame->dField.selectable_cards.clear();
		mainGame->dField.selected_cards.clear();
		uint32_t code;
		bool panelmode = false;
		bool select_ready = mainGame->dField.select_min == 0;
		mainGame->dField.select_ready = select_ready;
		ClientCard* pcard;
		for(uint32_t i = 0; i < count; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
			info.controler = mainGame->LocalPlayer(info.controler);
			if ((info.location & LOCATION_OVERLAY) > 0)
				pcard = mainGame->dField.GetCard(info.controler, info.location & (~LOCATION_OVERLAY) & 0xff, info.sequence)->overlayed[info.position];
			else if (info.location == 0) {
				pcard = new ClientCard{};
				pcard->sequence = static_cast<uint32_t>(mainGame->dField.limbo_temp.size());
				mainGame->dField.limbo_temp.push_back(pcard);
				panelmode = true;
			} else
				pcard = mainGame->dField.GetCard(info.controler, info.location, info.sequence);
			if (code != 0 && pcard->code != code)
				pcard->SetCode(code);
			pcard->select_seq = i;
			mainGame->dField.selectable_cards.push_back(pcard);
			pcard->is_selectable = true;
			pcard->is_selected = false;
			if (info.location & 0xf1)
				panelmode = true;
		}
		std::sort(mainGame->dField.selectable_cards.begin(), mainGame->dField.selectable_cards.end(), ClientCard::client_card_sort);
		std::wstring text = epro::format(L"{}({}-{})", gDataManager->GetDesc(select_hint ? select_hint : 560, mainGame->dInfo.compat_mode),
			mainGame->dField.select_min, mainGame->dField.select_max);
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		select_hint = 0;
		if (panelmode) {
			mainGame->wCardSelect->setText(text.data());
			mainGame->dField.ShowSelectCard(select_ready);
		} else {
			mainGame->stHintMsg->setText(text.data());
			mainGame->stHintMsg->setVisible(true);
		}
		if (mainGame->dField.select_cancelable) {
			mainGame->dField.ShowCancelOrFinishButton(1);
		} else if (select_ready) {
			mainGame->dField.ShowCancelOrFinishButton(2);
		} else {
			mainGame->dField.ShowCancelOrFinishButton(0);
		}
		return false;
	}
	case MSG_SELECT_UNSELECT_CARD: {
		/*uint8_t selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		bool finishable = BufferIO::Read<uint8_t>(pbuf) != 0;;
		bool cancelable = BufferIO::Read<uint8_t>(pbuf) != 0;
		mainGame->dField.select_cancelable = finishable || cancelable;
		mainGame->dField.select_min = CompatRead<uint8_t, uint32_t>(pbuf);
		mainGame->dField.select_max = CompatRead<uint8_t, uint32_t>(pbuf);
		uint32_t count1 = CompatRead<uint8_t, uint32_t>(pbuf);
		mainGame->dField.selectable_cards.clear();
		mainGame->dField.selected_cards.clear();
		uint32_t code;
		bool panelmode = false;
		mainGame->dField.select_ready = false;
		ClientCard* pcard;
		for(uint32_t i = 0; i < count1; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
			info.controler = mainGame->LocalPlayer(info.controler);
			if ((info.location & LOCATION_OVERLAY) > 0)
				pcard = mainGame->dField.GetCard(info.controler, info.location & (~LOCATION_OVERLAY) & 0xff, info.sequence)->overlayed[info.position];
			else if (info.location == 0) {
				pcard = new ClientCard{};
				pcard->sequence = static_cast<uint32_t>(mainGame->dField.limbo_temp.size());
				mainGame->dField.limbo_temp.push_back(pcard);
				panelmode = true;
			} else
				pcard = mainGame->dField.GetCard(info.controler, info.location, info.sequence);
			if (code != 0 && pcard->code != code)
				pcard->SetCode(code);
			pcard->select_seq = i;
			mainGame->dField.selectable_cards.push_back(pcard);
			pcard->is_selectable = true;
			pcard->is_selected = false;
			if (info.location & 0xf1)
				panelmode = true;
		}
		uint32_t count2 = CompatRead<uint8_t, uint32_t>(pbuf);
		for(uint32_t i = count1; i < count1 + count2; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
			info.controler = mainGame->LocalPlayer(info.controler);
			if ((info.location & LOCATION_OVERLAY) > 0)
				pcard = mainGame->dField.GetCard(info.controler, info.location & (~LOCATION_OVERLAY) & 0xff, info.sequence)->overlayed[info.position];
			else if (info.location == 0) {
				pcard = new ClientCard{};
				pcard->sequence = static_cast<uint32_t>(mainGame->dField.limbo_temp.size());
				mainGame->dField.limbo_temp.push_back(pcard);
				panelmode = true;
			} else
				pcard = mainGame->dField.GetCard(info.controler, info.location, info.sequence);
			if (code != 0 && pcard->code != code)
				pcard->SetCode(code);
			pcard->select_seq = i;
			mainGame->dField.selectable_cards.push_back(pcard);
			pcard->is_selectable = true;
			pcard->is_selected = true;
			if (info.location & 0xf1)
				panelmode = true;
		}
		std::sort(mainGame->dField.selectable_cards.begin(), mainGame->dField.selectable_cards.end(), ClientCard::client_card_sort);
		std::wstring text = epro::format(L"{}({}-{})", gDataManager->GetDesc(select_hint ? select_hint : 560, mainGame->dInfo.compat_mode),
			mainGame->dField.select_min, mainGame->dField.select_max);
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		select_hint = 0;
		if (panelmode) {
			mainGame->wCardSelect->setText(text.data());
			mainGame->dField.ShowSelectCard(mainGame->dField.select_cancelable);
		} else {
			mainGame->stHintMsg->setText(text.data());
			mainGame->stHintMsg->setVisible(true);
		}
		if (mainGame->dField.select_cancelable) {
			if (count2 == 0)
				mainGame->dField.ShowCancelOrFinishButton(1);
			else
				mainGame->dField.ShowCancelOrFinishButton(2);
		}
		else
			mainGame->dField.ShowCancelOrFinishButton(0);
		return false;
	}
	case MSG_SELECT_CHAIN: {
		/*uint8_t selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		uint32_t count{};
		if(mainGame->dInfo.compat_mode)
			count = BufferIO::Read<uint8_t>(pbuf);
		const auto specount = BufferIO::Read<uint8_t>(pbuf);
		const auto forced = BufferIO::Read<uint8_t>(pbuf);
		/*uint32_t hint0 = */BufferIO::Read<uint32_t>(pbuf);
		/*uint32_t hint1 = */BufferIO::Read<uint32_t>(pbuf);
		if(!mainGame->dInfo.compat_mode)
			count = BufferIO::Read<uint32_t>(pbuf);
		uint32_t code;
		uint64_t desc;
		ClientCard* pcard;
		bool panelmode = false;
		bool conti_exist = false;
		bool select_trigger = (specount == 0x7f);
		mainGame->dField.chain_forced = (forced != 0);
		mainGame->dField.activatable_cards.clear();
		mainGame->dField.activatable_descs.clear();
		mainGame->dField.conti_cards.clear();
		for(uint32_t i = 0; i < count; ++i) {
			uint8_t flag;
			if(mainGame->dInfo.compat_mode)
				flag = BufferIO::Read<uint8_t>(pbuf);
			code = BufferIO::Read<uint32_t>(pbuf);
			CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
			info.controler = mainGame->LocalPlayer(info.controler);
			desc = CompatRead<uint32_t, uint64_t>(pbuf);
			if(!mainGame->dInfo.compat_mode)
				flag = BufferIO::Read<uint8_t>(pbuf);
			pcard = mainGame->dField.GetCard(info.controler, info.location, info.sequence, info.position);
			if(!pcard) {
				pcard = new ClientCard{};
				pcard->code = code;
				pcard->controler = info.controler;
				pcard->sequence = static_cast<uint32_t>(mainGame->dField.limbo_temp.size());
				mainGame->dField.limbo_temp.push_back(pcard);
			}
			mainGame->dField.activatable_cards.push_back(pcard);
			mainGame->dField.activatable_descs.push_back(std::make_pair(desc, flag));
			pcard->is_selected = false;
			if(flag == EFFECT_CLIENT_MODE_RESOLVE) {
				pcard->chain_code = code;
				mainGame->dField.conti_cards.push_back(pcard);
				mainGame->dField.conti_act = true;
				conti_exist = true;
			} else {
				pcard->is_selectable = true;
				if(flag == EFFECT_CLIENT_MODE_RESET)
					pcard->cmdFlag |= COMMAND_RESET;
				else
					pcard->cmdFlag |= COMMAND_ACTIVATE;
				if(pcard->location == LOCATION_DECK) {
					pcard->SetCode(code);
					mainGame->dField.deck_act[pcard->controler] = true;
				} else if(info.location == LOCATION_GRAVE)
					mainGame->dField.grave_act[pcard->controler] = true;
				else if(info.location == LOCATION_REMOVED)
					mainGame->dField.remove_act[pcard->controler] = true;
				else if(info.location == LOCATION_EXTRA)
					mainGame->dField.extra_act[pcard->controler] = true;
				else if(info.location == LOCATION_OVERLAY)
					panelmode = true;
			}
		}
		const auto ignore_chain = mainGame->btnChainIgnore->isPressed();
		const auto always_chain = mainGame->btnChainAlways->isPressed();
		const auto chain_when_avail = mainGame->btnChainWhenAvail->isPressed();
		if(!select_trigger && !forced && (ignore_chain || ((count == 0 || specount == 0) && !always_chain)) && (count == 0 || !chain_when_avail)) {
			SetResponseI(-1);
			mainGame->dField.ClearChainSelect();
			if(mainGame->tabSettings.chkNoChainDelay->isChecked() && !ignore_chain) {
				std::unique_lock<epro::mutex> tmp(mainGame->gMutex);
				mainGame->WaitFrameSignal(20, tmp);
			}
			DuelClient::SendResponse();
			return true;
		}
		if(mainGame->tabSettings.chkAutoChainOrder->isChecked() && forced && !(always_chain || chain_when_avail)) {
			SetResponseI(0);
			mainGame->dField.ClearChainSelect();
			DuelClient::SendResponse();
			return true;
		}
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		if(!conti_exist)
			mainGame->stHintMsg->setText(gDataManager->GetSysString(550).data());
		else
			mainGame->stHintMsg->setText(gDataManager->GetSysString(556).data());
		mainGame->stHintMsg->setVisible(true);
		if(panelmode) {
			mainGame->dField.list_command = COMMAND_ACTIVATE;
			mainGame->dField.selectable_cards = mainGame->dField.activatable_cards;
			std::sort(mainGame->dField.selectable_cards.begin(), mainGame->dField.selectable_cards.end());
			auto eit = std::unique(mainGame->dField.selectable_cards.begin(), mainGame->dField.selectable_cards.end());
			mainGame->dField.selectable_cards.erase(eit, mainGame->dField.selectable_cards.end());
			mainGame->dField.ShowChainCard();
		} else {
			if(!forced) {
				if(count == 0)
					mainGame->stQMessage->setText(epro::format(L"{}\n{}", gDataManager->GetSysString(201), gDataManager->GetSysString(202)).data());
				else if(select_trigger)
					mainGame->stQMessage->setText(epro::format(L"{}\n{}\n{}", event_string, gDataManager->GetSysString(222), gDataManager->GetSysString(223)).data());
				else
					mainGame->stQMessage->setText(epro::format(L"{}\n{}", event_string, gDataManager->GetSysString(203)).data());
				mainGame->PopupElement(mainGame->wQuery);
			}
		}
		return false;
	}
	case MSG_SELECT_PLACE:
	case MSG_SELECT_DISFIELD: {
		uint8_t selecting_player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		//fluo passes this as 0 if the selection can be "canceled", ignore for now
		mainGame->dField.select_min = std::max<uint8_t>(BufferIO::Read<uint8_t>(pbuf), 1);
		uint32_t flag = BufferIO::Read<uint32_t>(pbuf);
		if(selecting_player == 1) {
			flag = flag << 16 | flag >> 16;
		}
		mainGame->dField.selectable_field = ~flag;
		mainGame->dField.selected_field = 0;
		uint8_t respbuf[64];
		bool pzone = false;
		std::wstring text;
		if (mainGame->dInfo.curMsg == MSG_SELECT_PLACE) {
			if (select_hint) {
				text = epro::sprintf(gDataManager->GetSysString(569), gDataManager->GetName(select_hint));
			} else
				text = gDataManager->GetDesc(560, mainGame->dInfo.compat_mode).data();
		} else
			text = gDataManager->GetDesc(select_hint ? select_hint : 570, mainGame->dInfo.compat_mode).data();
		select_hint = 0;
		mainGame->stHintMsg->setText(text.data());
		mainGame->stHintMsg->setVisible(true);
		if (mainGame->dInfo.curMsg == MSG_SELECT_PLACE && (
			(mainGame->gSettings.chkMAutoPos->isChecked() && mainGame->dField.selectable_field & 0x7f007f) ||
			(mainGame->gSettings.chkSTAutoPos->isChecked() && !(mainGame->dField.selectable_field & 0x7f007f)))) {
			if(mainGame->gSettings.chkRandomPos->isChecked()) {
				std::vector<char> positions;
				for(char i = 0; i < 32; i++) {
					if(mainGame->dField.selectable_field & (1 << i))
						positions.push_back(i);
				}
				char res = positions[(std::uniform_int_distribution<>(0, static_cast<int>(positions.size() - 1)))(rnd)];
				respbuf[0] = mainGame->LocalPlayer((res < 16) ? 0 : 1);
				respbuf[1] = ((1 << res) & 0x7f007f) ? LOCATION_MZONE : LOCATION_SZONE;
				respbuf[2] = (res%16) - (2 * (respbuf[1] - LOCATION_MZONE));
			} else {
				uint32_t filter;
				if(mainGame->dField.selectable_field & 0x7f) {
					respbuf[0] = mainGame->LocalPlayer(0);
					respbuf[1] = LOCATION_MZONE;
					filter = mainGame->dField.selectable_field & 0x7f;
				} else if(mainGame->dField.selectable_field & 0x1f00) {
					respbuf[0] = mainGame->LocalPlayer(0);
					respbuf[1] = LOCATION_SZONE;
					filter = (mainGame->dField.selectable_field >> 8) & 0x1f;
				} else if(mainGame->dField.selectable_field & 0xc000) {
					respbuf[0] = mainGame->LocalPlayer(0);
					respbuf[1] = LOCATION_SZONE;
					filter = (mainGame->dField.selectable_field >> 14) & 0x3;
					pzone = true;
				} else if(mainGame->dField.selectable_field & 0x7f0000) {
					respbuf[0] = mainGame->LocalPlayer(1);
					respbuf[1] = LOCATION_MZONE;
					filter = (mainGame->dField.selectable_field >> 16) & 0x7f;
				} else if(mainGame->dField.selectable_field & 0x1f000000) {
					respbuf[0] = mainGame->LocalPlayer(1);
					respbuf[1] = LOCATION_SZONE;
					filter = (mainGame->dField.selectable_field >> 24) & 0x1f;
				} else {
					respbuf[0] = mainGame->LocalPlayer(1);
					respbuf[1] = LOCATION_SZONE;
					filter = (mainGame->dField.selectable_field >> 30) & 0x3;
					pzone = true;
				}
				if(!pzone) {
					if(filter & 0x40) respbuf[2] = 6;
					else if(filter & 0x20) respbuf[2] = 5;
					else if(filter & 0x4) respbuf[2] = 2;
					else if(filter & 0x2) respbuf[2] = 1;
					else if(filter & 0x8) respbuf[2] = 3;
					else if(filter & 0x1) respbuf[2] = 0;
					else if(filter & 0x10) respbuf[2] = 4;
				} else {
					if(filter & 0x1) respbuf[2] = 6;
					else if(filter & 0x2) respbuf[2] = 7;
				}
			}
			mainGame->dField.selectable_field = 0;
			SetResponseB(respbuf, 3);
			DuelClient::SendResponse();
			return true;
		}
		return false;
	}
	case MSG_SELECT_POSITION: {
		/*uint8_t selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		uint32_t code = BufferIO::Read<uint32_t>(pbuf);
		uint8_t positions = BufferIO::Read<uint8_t>(pbuf);
		if (positions == POS_FACEUP_ATTACK || positions == POS_FACEDOWN_ATTACK || positions == POS_FACEUP_DEFENSE || positions == POS_FACEDOWN_DEFENSE) {
			SetResponseI(positions);
			return true;
		}
		int count = 0, filter = 0x1, startpos;
		while(filter != 0x10) {
			if(positions & filter) count++;
			filter <<= 1;
		}
		if(count == 4) startpos = 10;
		else if(count == 3) startpos = 82;
		else startpos = 155;
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		if(positions & POS_FACEUP_ATTACK) {
			mainGame->imageLoading[mainGame->btnPSAU] = code;
			mainGame->btnPSAU->setRelativePosition(mainGame->Scale<irr::s32>(startpos, 45, startpos + 140, 185));
			mainGame->btnPSAU->setVisible(true);
			startpos += 145;
		} else mainGame->btnPSAU->setVisible(false);
		if(positions & POS_FACEDOWN_ATTACK) {
			mainGame->btnPSAD->setRelativePosition(mainGame->Scale<irr::s32>(startpos, 45, startpos + 140, 185));
			mainGame->btnPSAD->setVisible(true);
			startpos += 145;
		} else mainGame->btnPSAD->setVisible(false);
		if(positions & POS_FACEUP_DEFENSE) {
			mainGame->imageLoading[mainGame->btnPSDU] = code;
			mainGame->btnPSDU->setRelativePosition(mainGame->Scale<irr::s32>(startpos, 45, startpos + 140, 185));
			mainGame->btnPSDU->setVisible(true);
			startpos += 145;
		} else mainGame->btnPSDU->setVisible(false);
		if(positions & POS_FACEDOWN_DEFENSE) {
			mainGame->btnPSDD->setRelativePosition(mainGame->Scale<irr::s32>(startpos, 45, startpos + 140, 185));
			mainGame->btnPSDD->setVisible(true);
			startpos += 145;
		} else mainGame->btnPSDD->setVisible(false);
		mainGame->PopupElement(mainGame->wPosSelect);
		return false;
	}
	case MSG_SELECT_TRIBUTE: {
		/*uint8_t selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		mainGame->dField.select_cancelable = BufferIO::Read<uint8_t>(pbuf) != 0;
		mainGame->dField.select_min = CompatRead<uint8_t, uint32_t>(pbuf);
		mainGame->dField.select_max = CompatRead<uint8_t, uint32_t>(pbuf);
		uint32_t count = CompatRead<uint8_t, uint32_t>(pbuf);
		mainGame->dField.selectable_cards.clear();
		mainGame->dField.selected_cards.clear();
		uint32_t code, s;
		uint8_t c, l, t;
		ClientCard* pcard;
		mainGame->dField.select_ready = false;
		for(uint32_t i = 0; i < count; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			c = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			l = BufferIO::Read<uint8_t>(pbuf);
			s = CompatRead<uint8_t, uint32_t>(pbuf);
			t = BufferIO::Read<uint8_t>(pbuf);
			pcard = mainGame->dField.GetCard(c, l, s);
			if (code && pcard->code != code)
				pcard->SetCode(code);
			mainGame->dField.selectable_cards.push_back(pcard);
			pcard->opParam = t;
			pcard->select_seq = i;
			pcard->is_selectable = true;
		}
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->stHintMsg->setText(epro::format(L"{}({}-{})", gDataManager->GetDesc(select_hint ? select_hint : 531, mainGame->dInfo.compat_mode), mainGame->dField.select_min, mainGame->dField.select_max).data());
		mainGame->stHintMsg->setVisible(true);
		if (mainGame->dField.select_cancelable) {
			mainGame->dField.ShowCancelOrFinishButton(1);
		}
		select_hint = 0;
		return false;
	}
	case MSG_SELECT_COUNTER: {
		/*uint8_t selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		mainGame->dField.select_counter_type = BufferIO::Read<uint16_t>(pbuf);
		mainGame->dField.select_counter_count = BufferIO::Read<uint16_t>(pbuf);
		uint32_t count = CompatRead<uint8_t, uint32_t>(pbuf);
		mainGame->dField.selectable_cards.clear();
		/*uint32_t code;*/
		uint16_t t;
		uint8_t c, l, s;
		ClientCard* pcard;
		for(uint32_t i = 0; i < count; ++i) {
			/*code = */BufferIO::Read<uint32_t>(pbuf);
			c = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			l = BufferIO::Read<uint8_t>(pbuf);
			s = BufferIO::Read<uint8_t>(pbuf);
			t = BufferIO::Read<uint16_t>(pbuf);
			pcard = mainGame->dField.GetCard(c, l, s);
			mainGame->dField.selectable_cards.push_back(pcard);
			pcard->opParam = (t << 16) | t;
			pcard->is_selectable = true;
		}
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->stHintMsg->setText(epro::sprintf(gDataManager->GetSysString(204), mainGame->dField.select_counter_count, gDataManager->GetCounterName(mainGame->dField.select_counter_type)).data());
		mainGame->stHintMsg->setVisible(true);
		return false;
	}
	case MSG_SELECT_SUM: {
		auto GetCard = [&] {
			CoreUtils::loc_info info{};
			if(mainGame->dInfo.compat_mode) {
				uint8_t c = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
				uint8_t l = BufferIO::Read<uint8_t>(pbuf);
				uint32_t s = CompatRead<uint8_t, uint32_t>(pbuf);
				info.controler = c;
				info.location = l;
				info.sequence = s;
			} else {
				info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
				info.controler = mainGame->LocalPlayer(info.controler);
			}
			if((info.location & LOCATION_OVERLAY) > 0)
				return mainGame->dField.GetCard(info.controler, info.location & (~LOCATION_OVERLAY) & 0xff, info.sequence)->overlayed[info.position];
			return mainGame->dField.GetCard(info.controler, info.location, info.sequence);
		};
		/*uint8_t selecting_player*/
		if(mainGame->dInfo.compat_mode) {
			mainGame->dField.select_mode = BufferIO::Read<uint8_t>(pbuf);
			/*selecting_player = */BufferIO::Read<uint8_t>(pbuf);
		} else {
			/*selecting_player = */BufferIO::Read<uint8_t>(pbuf);
			mainGame->dField.select_mode = BufferIO::Read<uint8_t>(pbuf);
		}
		mainGame->dField.select_sumval = BufferIO::Read<uint32_t>(pbuf);
		mainGame->dField.select_min = CompatRead<uint8_t, uint32_t>(pbuf);
		mainGame->dField.select_max = CompatRead<uint8_t, uint32_t>(pbuf);
		mainGame->dField.must_select_count = CompatRead<uint8_t, uint32_t>(pbuf);
		mainGame->dField.selectable_cards.clear();
		mainGame->dField.selectsum_all.clear();
		mainGame->dField.selected_cards.clear();
		mainGame->dField.must_select_cards.clear();
		mainGame->dField.selectsum_cards.clear();
		for (uint32_t i = 0; i < mainGame->dField.must_select_count; ++i) {
			uint32_t code = BufferIO::Read<uint32_t>(pbuf);
			ClientCard* pcard = GetCard();
			if (code != 0 && pcard->code != code)
				pcard->SetCode(code);
			pcard->opParam = BufferIO::Read<uint32_t>(pbuf);
			pcard->select_seq = 0;
			mainGame->dField.must_select_cards.push_back(pcard);
		}
		uint32_t count = CompatRead<uint8_t, uint32_t>(pbuf);
		for(uint32_t i = 0; i < count; ++i) {
			uint32_t code = BufferIO::Read<uint32_t>(pbuf);
			ClientCard* pcard = GetCard();
			if (code != 0 && pcard->code != code)
				pcard->SetCode(code);
			pcard->opParam = BufferIO::Read<uint32_t>(pbuf);
			pcard->select_seq = i;
			mainGame->dField.selectsum_all.push_back(pcard);
		}
		std::sort(mainGame->dField.selectsum_all.begin(), mainGame->dField.selectsum_all.end(), ClientCard::client_card_sort);
		std::wstring text = epro::format(L"{}({})", gDataManager->GetDesc(select_hint ? select_hint : 560, mainGame->dInfo.compat_mode), mainGame->dField.select_sumval);
		select_hint = 0;
		mainGame->wCardSelect->setText(text.data());
		mainGame->stHintMsg->setText(text.data());
		return mainGame->dField.ShowSelectSum();
	}
	case MSG_SORT_CARD:
	case MSG_SORT_CHAIN: {
		/*const auto player = */BufferIO::Read<uint8_t>(pbuf);
		const auto count = CompatRead<uint8_t, uint32_t>(pbuf);
		mainGame->dField.selectable_cards.clear();
		mainGame->dField.selected_cards.clear();
		mainGame->dField.sort_list.clear();
		ClientCard* pcard;
		for(uint32_t i = 0; i < count; ++i) {
			const auto code = BufferIO::Read<uint32_t>(pbuf);
			const auto c = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			const auto l = CompatRead<uint8_t, uint32_t>(pbuf);
			const auto s = CompatRead<uint8_t, uint32_t>(pbuf);
			pcard = mainGame->dField.GetCard(c, l, s);
			if (code != 0 && pcard->code != code)
				pcard->SetCode(code);
			mainGame->dField.selectable_cards.push_back(pcard);
			mainGame->dField.sort_list.push_back(0);
		}
		if (mainGame->tabSettings.chkAutoChainOrder->isChecked() && mainGame->dInfo.curMsg == MSG_SORT_CHAIN) {
			mainGame->dField.sort_list.clear();
			SetResponseI(-1);
			DuelClient::SendResponse();
			return true;
		}
		if(mainGame->dInfo.curMsg == MSG_SORT_CHAIN)
			mainGame->wCardSelect->setText(gDataManager->GetSysString(206).data());
		else
			mainGame->wCardSelect->setText(gDataManager->GetSysString(205).data());
		mainGame->dField.select_min = 0;
		mainGame->dField.select_max = count;
		mainGame->dField.ShowSelectCard();
		return false;
	}
	case MSG_CONFIRM_DECKTOP: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto count = CompatRead<uint8_t, uint32_t>(pbuf);
		uint32_t code;
		ClientCard* pcard;
		mainGame->dField.selectable_cards.clear();
		for(uint32_t i = 0; i < count; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			pbuf += (mainGame->dInfo.compat_mode) ? 3 : 6;
			pcard = *(mainGame->dField.deck[player].rbegin() + i);
			if (code != 0)
				pcard->SetCode(code);
		}
		if(mainGame->dInfo.isCatchingUp)
			return true;
		mainGame->AddLog(epro::sprintf(gDataManager->GetSysString(207), count));
		constexpr float milliseconds = 5.0f * 1000.0f / 60.0f;
		float shift = -0.75f / milliseconds;
		if(!!player ^ mainGame->current_topdown) shift *= -1.0f;
		for(auto it = mainGame->dField.deck[player].crbegin(), end = it + count; it != end; ++it) {
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			pcard = *it;
			mainGame->AddLog(epro::format(L"*[{}]", gDataManager->GetName(pcard->code)), pcard->code);
			pcard->dPos.set(shift, 0, 0);
			if(!mainGame->dField.deck_reversed && !pcard->is_reversed)
				pcard->dRot.set(0, irr::core::PI / milliseconds, 0);
			else pcard->dRot.set(0, 0, 0);
			pcard->is_moving = true;
			pcard->aniFrame = milliseconds;
			mainGame->WaitFrameSignal(45, lock);
			mainGame->dField.MoveCard(pcard, 5);
			mainGame->WaitFrameSignal(5, lock);
		}
		return true;
	}
	case MSG_CONFIRM_EXTRATOP: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto count = CompatRead<uint8_t, uint32_t>(pbuf);
		uint32_t code;
		ClientCard* pcard;
		mainGame->dField.selectable_cards.clear();
		for(auto it = mainGame->dField.extra[player].crbegin(), end = it + count; it != end; ++it) {
			code = BufferIO::Read<uint32_t>(pbuf);
			pbuf += (mainGame->dInfo.compat_mode) ? 3 : 6;
			pcard = *it;
			if (code != 0)
				pcard->SetCode(code);
		}
		if(mainGame->dInfo.isCatchingUp)
			return true;
		mainGame->AddLog(epro::sprintf(gDataManager->GetSysString(207), count));
		for(auto it = mainGame->dField.extra[player].crbegin(), end = it + count; it != end; ++it) {
			pcard = *it;
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->AddLog(epro::format(L"*[{}]", gDataManager->GetName(pcard->code)), pcard->code);
			constexpr float milliseconds = 5.0f * 1000.0f / 60.0f;
			if (player == 0)
				pcard->dPos.set(0, -1.0f / milliseconds, 0);
			else
				pcard->dPos.set(0.75f / milliseconds, 0, 0);
			pcard->dRot.set(0, irr::core::PI / milliseconds, 0);
			pcard->is_moving = true;
			pcard->aniFrame = milliseconds;
			mainGame->WaitFrameSignal(45, lock);
			mainGame->dField.MoveCard(pcard, 5);
			mainGame->WaitFrameSignal(5, lock);
		}
		return true;
	}
	case MSG_CONFIRM_CARDS: {
		/*const auto player = */mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto count = CompatRead<uint8_t, uint32_t>(pbuf);
		uint32_t code, l, s;
		uint8_t c;
		std::vector<ClientCard*> field_confirm;
		std::vector<ClientCard*> panel_confirm;
		if(mainGame->dInfo.isCatchingUp)
			return true;
		mainGame->AddLog(epro::sprintf(gDataManager->GetSysString(208), count));
		for(uint32_t i = 0; i < count; ++i) {
			code = BufferIO::Read<uint32_t>(pbuf);
			c = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
			l = BufferIO::Read<uint8_t>(pbuf);
			s = CompatRead<uint8_t, uint32_t>(pbuf);
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			ClientCard* pcard;
			if (l == 0) {
				pcard = new ClientCard{};
				pcard->sequence = static_cast<uint32_t>(mainGame->dField.limbo_temp.size());
				mainGame->dField.limbo_temp.push_back(pcard);
			} else
				pcard = mainGame->dField.GetCard(c, l, s);
			if (code != 0)
				pcard->SetCode(code);
			mainGame->AddLog(epro::format(L"*[{}]", gDataManager->GetName(code)), code);
			if (l & (LOCATION_EXTRA | LOCATION_DECK) || l == 0) {
				if(count == 1 && l != 0) {
					constexpr float milliseconds = 5.0f * 1000.0f / 60.0f;
					float shift = -0.75f / milliseconds;
					if (c == 0 && l == LOCATION_EXTRA) shift *= -1.0f;
					pcard->dPos.set(shift, 0, 0);
					if(((l == LOCATION_DECK) && mainGame->dField.deck_reversed) || pcard->is_reversed || (pcard->position & POS_FACEUP))
						pcard->dRot.set(0, 0, 0);
					else pcard->dRot.set(0, irr::core::PI / milliseconds, 0);
					pcard->is_moving = true;
					pcard->aniFrame = milliseconds;
					mainGame->WaitFrameSignal(45, lock);
					mainGame->dField.MoveCard(pcard, 5);
					mainGame->WaitFrameSignal(5, lock);
				} else {
					if(!mainGame->dInfo.isReplay)
						panel_confirm.push_back(pcard);
				}
			} else {
				if(!mainGame->dInfo.isReplay || (l & LOCATION_ONFIELD) || (l & LOCATION_HAND && gGameConfig->hideHandsInReplays))
					field_confirm.push_back(pcard);
			}
		}
		if (field_confirm.size() > 0) {
			std::map<ClientCard*, bool> public_status;
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->WaitFrameSignal(5, lock);
			for(auto& pcard : field_confirm) {
				l = pcard->location;
				if (l == LOCATION_HAND) {
					if(mainGame->dInfo.isReplay) {
						public_status[pcard] = pcard->is_public;
						pcard->is_public = true;
					}
					mainGame->dField.MoveCard(pcard, 5);
					pcard->is_highlighting = true;
				} else if (l == LOCATION_MZONE) {
					if (pcard->position & POS_FACEUP)
						continue;
					constexpr float milliseconds = 5.0f * 1000.0f / 60.0f;
					pcard->dPos.set(0, 0, 0);
					if (pcard->position == POS_FACEDOWN_ATTACK)
						pcard->dRot.set(0, irr::core::PI / milliseconds, 0);
					else
						pcard->dRot.set(irr::core::PI / milliseconds, 0, 0);
					pcard->is_moving = true;
					pcard->aniFrame = milliseconds;
				} else if (l == LOCATION_SZONE) {
					if (pcard->position & POS_FACEUP)
						continue;
					constexpr float milliseconds = 5.0f * 1000.0f / 60.0f;
					pcard->dPos.set(0, 0, 0);
					pcard->dRot.set(0, irr::core::PI / milliseconds, 0);
					pcard->is_moving = true;
					pcard->aniFrame = milliseconds;
				}
			}
			if (mainGame->dInfo.isReplay)
				mainGame->WaitFrameSignal(30, lock);
			else
				mainGame->WaitFrameSignal(90, lock);
			for(auto& pcard : field_confirm) {
				if(mainGame->dInfo.isReplay && pcard->location & LOCATION_HAND)
					pcard->is_public = public_status[pcard];
				mainGame->dField.MoveCard(pcard, 5);
				pcard->is_highlighting = false;
			}
			mainGame->WaitFrameSignal(5, lock);
		}
		if (panel_confirm.size()) {
			std::sort(panel_confirm.begin(), panel_confirm.end(), ClientCard::client_card_sort);
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->dField.selectable_cards = panel_confirm;
			mainGame->wCardSelect->setText(epro::sprintf(gDataManager->GetSysString(208), panel_confirm.size()).data());
			mainGame->dField.ShowSelectCard(true);
			mainGame->actionSignal.Wait(lock);
		}
		return true;
	}
	case MSG_SHUFFLE_DECK: {
		Play(SoundManager::SFX::SHUFFLE);
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		if(mainGame->dField.deck[player].size() < 2)
			return true;
		bool rev = mainGame->dField.deck_reversed;
		auto lock = LockIf();
		if(!mainGame->dInfo.isCatchingUp) {
			mainGame->dField.deck_reversed = false;
			if(rev) {
				for(const auto& pcard : mainGame->dField.deck[player])
					mainGame->dField.MoveCard(pcard, 10);
				mainGame->WaitFrameSignal(10, lock);
			}
		}
		for(const auto& pcard : mainGame->dField.deck[player]) {
			pcard->code = 0;
			pcard->is_reversed = false;
		}
		if(!mainGame->dInfo.isCatchingUp) {
			for(int i = 0; i < 5; ++i) {
				for(const auto& pcard : mainGame->dField.deck[player]) {
					constexpr float milliseconds = 3.0f * 1000.0f / 60.0f;
					pcard->dPos.set((rand() * 1.2f / RAND_MAX - 0.2f) / milliseconds, 0, 0);
					pcard->dRot.set(0, 0, 0);
					pcard->is_moving = true;
					pcard->aniFrame = milliseconds;
				}
				mainGame->WaitFrameSignal(3, lock);
				for(const auto& pcard : mainGame->dField.deck[player])
					mainGame->dField.MoveCard(pcard, 3);
				mainGame->WaitFrameSignal(3, lock);
			}
			mainGame->dField.deck_reversed = rev;
			if(rev) {
				for(const auto& pcard : mainGame->dField.deck[player])
					mainGame->dField.MoveCard(pcard, 10);
				mainGame->WaitFrameSignal(10, lock);
			}
		}
		return true;
	}
	case MSG_SHUFFLE_HAND: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		/*const auto count = */CompatRead<uint8_t, uint32_t>(pbuf);
		auto lock = LockIf();
		if(!mainGame->dInfo.isCatchingUp) {
			mainGame->WaitFrameSignal(5, lock);
			if(player == 1 && !mainGame->dInfo.isReplay && !mainGame->dInfo.isSingleMode) {
				bool flip = false;
				for(const auto& pcard : mainGame->dField.hand[player])
					if(pcard->code) {
						constexpr float milliseconds = 5.0f * 1000.0f / 60.0f;
						pcard->dPos.set(0, 0, 0);
						pcard->dRot.set(1.322f / milliseconds, irr::core::PI / milliseconds, 0);
						pcard->is_moving = true;
						pcard->is_hovered = false;
						pcard->aniFrame = milliseconds;
						flip = true;
					}
				if(flip)
					mainGame->WaitFrameSignal(5, lock);
			}
			for(const auto& pcard : mainGame->dField.hand[player]) {
				constexpr float milliseconds = 5.0f * 1000.0f / 60.0f;
				pcard->dPos.set((3.9f - pcard->curPos.X) / milliseconds, 0, 0);
				pcard->dRot.set(0, 0, 0);
				pcard->is_moving = true;
				pcard->is_hovered = false;
				pcard->aniFrame = milliseconds;
			}
			mainGame->WaitFrameSignal(11, lock);
		}
		for(const auto& pcard : mainGame->dField.hand[player]) {
			pcard->SetCode(BufferIO::Read<uint32_t>(pbuf));
			pcard->desc_hints.clear();
			if(!mainGame->dInfo.isCatchingUp) {
				pcard->is_hovered = false;
				mainGame->dField.MoveCard(pcard, 5);
			}
		}
		if(!mainGame->dInfo.isCatchingUp)
			mainGame->WaitFrameSignal(5, lock);
		return true;
	}
	case MSG_SHUFFLE_EXTRA: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto count = CompatRead<uint8_t, uint32_t>(pbuf);
		if((mainGame->dField.extra[player].size() - mainGame->dField.extra_p_count[player]) < 2)
			return true;
		auto lock = LockIf();
		if(!mainGame->dInfo.isCatchingUp) {
			if(count > 1)
				Play(SoundManager::SFX::SHUFFLE);
			for(int i = 0; i < 5; ++i) {
				for(const auto& pcard : mainGame->dField.extra[player]) {
					if(!(pcard->position & POS_FACEUP)) {
						constexpr float milliseconds = 3.0f * 1000.0f / 60.0f;
						pcard->dPos.set((rand() * 1.2f / RAND_MAX - 0.2f) / milliseconds, 0, 0);
						pcard->dRot.set(0, 0, 0);
						pcard->is_moving = true;
						pcard->aniFrame = milliseconds;
					}
				}
				mainGame->WaitFrameSignal(3, lock);
				for(const auto& pcard : mainGame->dField.extra[player])
					if(!(pcard->position & POS_FACEUP))
						mainGame->dField.MoveCard(pcard, 3);
				mainGame->WaitFrameSignal(3, lock);
			}
		}
		for (const auto& pcard : mainGame->dField.extra[player])
			if(!(pcard->position & POS_FACEUP))
				pcard->SetCode(BufferIO::Read<uint32_t>(pbuf));
		return true;
	}
	case MSG_REFRESH_DECK: {
		/*const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));*/
		return true;
	}
	case MSG_SWAP_GRAVE_DECK: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		ProgressiveBuffer buff;
		if(!mainGame->dInfo.compat_mode) {
			/*const auto mainsize = */BufferIO::Read<uint32_t>(pbuf);
			const auto extrabuffersize = BufferIO::Read<uint32_t>(pbuf);
			buff.data.resize(extrabuffersize);
			BufferIO::Read(pbuf, buff.data.data(), extrabuffersize);
		}
		auto checkextra = [&buff, compat = mainGame->dInfo.compat_mode] (int idx, ClientCard* pcard) -> bool {
			if(compat)
				return pcard->type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ | TYPE_LINK);
			return buff.bitGet(idx);
		};
		auto lock = LockIf();
		mainGame->dField.grave[player].swap(mainGame->dField.deck[player]);
		for(const auto& pcard : mainGame->dField.grave[player]) {
			pcard->location = LOCATION_GRAVE;
			if(!mainGame->dInfo.isCatchingUp)
				mainGame->dField.MoveCard(pcard, 10);
		}
		int m = 0;
		int i = 0;
		for(auto cit = mainGame->dField.deck[player].begin(); cit != mainGame->dField.deck[player].end(); i++) {
			ClientCard* pcard = *cit;
			if(checkextra(i, pcard)) {
				pcard->position = POS_FACEDOWN;
				mainGame->dField.AddCard(pcard, player, LOCATION_EXTRA, 0);
				cit = mainGame->dField.deck[player].erase(cit);
			} else {
				pcard->location = LOCATION_DECK;
				pcard->sequence = m++;
				++cit;
			}
			if(!mainGame->dInfo.isCatchingUp)
				mainGame->dField.MoveCard(pcard, 10);
		}
		if(!mainGame->dInfo.isCatchingUp)
			mainGame->WaitFrameSignal(11, lock);
		return true;
	}
	case MSG_REVERSE_DECK: {
		mainGame->dField.deck_reversed = !mainGame->dField.deck_reversed;
		if(!mainGame->dInfo.isCatchingUp) {
			for(auto& pcard : mainGame->dField.deck[0])
				mainGame->dField.MoveCard(pcard, 10);
			for(auto& pcard : mainGame->dField.deck[1])
				mainGame->dField.MoveCard(pcard, 10);
		}
		return true;
	}
	case MSG_DECK_TOP: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto seq = CompatRead<uint8_t, uint32_t>(pbuf);
		ClientCard* pcard = mainGame->dField.GetCard(player, LOCATION_DECK, mainGame->dField.deck[player].size() - 1 - seq);
		const auto code = BufferIO::Read<uint32_t>(pbuf);
		bool rev;
		if(!mainGame->dInfo.compat_mode) {
			rev = (BufferIO::Read<uint32_t>(pbuf) & POS_FACEUP_DEFENSE) != 0;
			pcard->SetCode(code);
		} else {
			rev = (code & 0x80000000) != 0;
			pcard->SetCode(code & 0x7fffffff);
		}
		if(pcard->is_reversed != rev) {
			pcard->is_reversed = rev;
			mainGame->dField.MoveCard(pcard, 5);
		}
		return true;
	}
	case MSG_SHUFFLE_SET_CARD: {
		std::vector<ClientCard*>* lst = 0;
		const auto loc = BufferIO::Read<uint8_t>(pbuf);
		const auto count = BufferIO::Read<uint8_t>(pbuf);
		if(loc == LOCATION_MZONE)
			lst = mainGame->dField.mzone;
		else
			lst = mainGame->dField.szone;
		ClientCard* mc[7];
		auto lock = LockIf();
		for (int i = 0; i < count; ++i) {
			CoreUtils::loc_info previous = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
			previous.controler = mainGame->LocalPlayer(previous.controler);
			mc[i] = lst[previous.controler][previous.sequence];
			mc[i]->SetCode(0);
			if(!mainGame->dInfo.isCatchingUp) {
				constexpr float milliseconds = 10.0f * 1000.0f / 60.0f;
				mc[i]->dPos.set((3.95f - mc[i]->curPos.X) / milliseconds, 0, 0.5f / milliseconds);
				mc[i]->dRot.set(0, 0, 0);
				mc[i]->is_moving = true;
				mc[i]->aniFrame = milliseconds;
			}
		}
		if(!mainGame->dInfo.isCatchingUp)
			mainGame->WaitFrameSignal(20, lock);
		for (int i = 0; i < count; ++i) {
			CoreUtils::loc_info current = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
			current.controler = mainGame->LocalPlayer(current.controler);
			if (current.location > 0) {
				uint8_t prev_seq = mc[i]->sequence;
				auto tmp = lst[current.controler][current.sequence];
				lst[current.controler][prev_seq] = tmp;
				lst[current.controler][current.sequence] = mc[i];
				mc[i]->sequence = current.sequence;
				tmp->sequence = prev_seq;
			}
		}
		if(!mainGame->dInfo.isCatchingUp) {
			for (int i = 0; i < count; ++i) {
				mainGame->dField.MoveCard(mc[i], 10);
				for (auto pcard : mc[i]->overlayed)
					mainGame->dField.MoveCard(pcard, 10);
			}
			mainGame->WaitFrameSignal(11, lock);
		}
		return true;
	}
	case MSG_NEW_TURN: {
		Play(SoundManager::SFX::NEXT_TURN);
		/*const auto player = */mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		auto lock = LockIf();
		mainGame->dInfo.turn++;
		if(!mainGame->dInfo.isReplay && !mainGame->dInfo.isSingleMode && mainGame->dInfo.player_type < (mainGame->dInfo.team1 + mainGame->dInfo.team2)) {
			mainGame->btnLeaveGame->setText(gDataManager->GetSysString(1351).data());
			mainGame->btnLeaveGame->setVisible(true);
		}
		if(!mainGame->dInfo.isReplay && mainGame->dInfo.player_type < 7) {
			if(!mainGame->tabSettings.chkHideChainButtons->isChecked()) {
				mainGame->btnChainIgnore->setVisible(true);
				mainGame->btnChainAlways->setVisible(true);
				mainGame->btnChainWhenAvail->setVisible(true);
				//mainGame->dField.UpdateChainButtons();
			} else {
				mainGame->btnChainIgnore->setVisible(false);
				mainGame->btnChainAlways->setVisible(false);
				mainGame->btnChainWhenAvail->setVisible(false);
				mainGame->btnCancelOrFinish->setVisible(false);
			}
		}
		if(!mainGame->dInfo.isCatchingUp) {
			mainGame->showcardcode = 10;
			mainGame->showcarddif = 30;
			mainGame->showcardp = 0;
			mainGame->showcard = 101;
			mainGame->WaitFrameSignal(40, lock);
			mainGame->showcard = 0;
		}
		return true;
	}
	case MSG_NEW_PHASE: {
		Play(SoundManager::SFX::PHASE);
		const auto phase = BufferIO::Read<uint16_t>(pbuf);
		auto lock = LockIf();
		mainGame->btnDP->setVisible(false);
		mainGame->btnDP->setSubElement(false);
		mainGame->btnSP->setVisible(false);
		mainGame->btnSP->setSubElement(false);
		mainGame->btnM1->setVisible(false);
		mainGame->btnM1->setSubElement(false);
		mainGame->btnBP->setVisible(false);
		mainGame->btnBP->setSubElement(false);
		mainGame->btnM2->setVisible(false);
		mainGame->btnM2->setSubElement(false);
		mainGame->btnEP->setVisible(false);
		mainGame->btnEP->setSubElement(false);
		if(gGameConfig->alternative_phase_layout) {
			mainGame->btnDP->setVisible(true);
			mainGame->btnSP->setVisible(true);
			mainGame->btnM1->setVisible(true);
			mainGame->btnBP->setVisible(true);
			mainGame->btnBP->setPressed(false);
			mainGame->btnBP->setEnabled(false);
			mainGame->btnM2->setVisible(true);
			mainGame->btnM2->setPressed(false);
			mainGame->btnM2->setEnabled(false);
			mainGame->btnEP->setVisible(true);
			mainGame->btnEP->setPressed(false);
			mainGame->btnEP->setEnabled(false);
		}
		mainGame->btnShuffle->setVisible(false);
		mainGame->showcarddif = 30;
		mainGame->showcardp = 0;
		switch (phase) {
		case PHASE_DRAW:
			event_string = gDataManager->GetSysString(20).data();
			mainGame->btnDP->setVisible(true);
			mainGame->btnDP->setSubElement(true);
			mainGame->showcardcode = 4;
			break;
		case PHASE_STANDBY:
			event_string = gDataManager->GetSysString(21).data();
			mainGame->btnSP->setVisible(true);
			mainGame->btnSP->setSubElement(true);
			mainGame->showcardcode = 5;
			break;
		case PHASE_MAIN1:
			event_string = gDataManager->GetSysString(22).data();
			mainGame->btnM1->setVisible(true);
			mainGame->btnM1->setSubElement(true);
			mainGame->showcardcode = 6;
			break;
		case PHASE_BATTLE_START:
			event_string = gDataManager->GetSysString(24).data();
			mainGame->btnBP->setVisible(true);
			mainGame->btnBP->setSubElement(true);
			mainGame->btnBP->setPressed(true);
			mainGame->btnBP->setEnabled(false);
			mainGame->showcardcode = 7;
			break;
		case PHASE_MAIN2:
			event_string = gDataManager->GetSysString(22).data();
			mainGame->btnM2->setVisible(true);
			mainGame->btnM2->setSubElement(true);
			mainGame->btnM2->setPressed(true);
			mainGame->btnM2->setEnabled(false);
			mainGame->showcardcode = 8;
			break;
		case PHASE_END:
			event_string = gDataManager->GetSysString(26).data();
			mainGame->btnEP->setVisible(true);
			mainGame->btnEP->setSubElement(true);
			mainGame->btnEP->setPressed(true);
			mainGame->btnEP->setEnabled(false);
			mainGame->showcardcode = 9;
			break;
		}
		if(!mainGame->dInfo.isCatchingUp) {
			mainGame->showcard = 101;
			mainGame->WaitFrameSignal(40, lock);
			mainGame->showcard = 0;
		}
		return true;
	}
	case MSG_MOVE: {
		const auto code = BufferIO::Read<uint32_t>(pbuf);
		CoreUtils::loc_info previous = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		previous.controler = mainGame->LocalPlayer(previous.controler);
		CoreUtils::loc_info current = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		current.controler = mainGame->LocalPlayer(current.controler);
		const auto reason = BufferIO::Read<uint32_t>(pbuf);
		if (previous.location != current.location) {
			if (reason & REASON_DESTROY)
				Play(SoundManager::SFX::DESTROYED);
			else if (current.location & LOCATION_REMOVED)
				Play(SoundManager::SFX::BANISHED);
		}
		auto lock = LockIf();
		if (previous.location == 0) {
			ClientCard* pcard = new ClientCard{};
			pcard->position = current.position;
			pcard->SetCode(code);
			mainGame->dField.AddCard(pcard, current.controler, current.location, current.sequence);
			if(!mainGame->dInfo.isCatchingUp) {
				pcard->UpdateDrawCoordinates(true);
				pcard->curAlpha = 5;
				int frames = 20;
				if(gGameConfig->quick_animation)
					frames = 12;
				mainGame->dField.FadeCard(pcard, 255, frames);
				mainGame->WaitFrameSignal(frames, lock);
			}
		} else if (current.location == 0) {
			ClientCard* pcard = nullptr;
			if(previous.location & LOCATION_OVERLAY) {
				auto olcard = mainGame->dField.GetCard(previous.controler, (previous.location & (~LOCATION_OVERLAY)) & 0xff, previous.sequence);
				pcard = olcard->overlayed[previous.position];
			} else
				pcard = mainGame->dField.GetCard(previous.controler, previous.location, previous.sequence);
			if (code != 0 && pcard->code != code)
				pcard->SetCode(code);
			pcard->ClearTarget();
			for(const auto& eqcard : pcard->equipped)
				eqcard->equipTarget = nullptr;
			if(!mainGame->dInfo.isCatchingUp) {
				int frames = 20;
				if(gGameConfig->quick_animation)
					frames = 12;
				mainGame->dField.FadeCard(pcard, 5, frames);
				mainGame->WaitFrameSignal(frames, lock);
			}
			if(pcard->location & LOCATION_OVERLAY) {
				pcard->overlayTarget->overlayed.erase(pcard->overlayTarget->overlayed.begin() + pcard->sequence);
				pcard->overlayTarget = 0;
				mainGame->dField.overlay_cards.erase(pcard);
			} else
				mainGame->dField.RemoveCard(previous.controler, previous.location, previous.sequence);
			if(!mainGame->dInfo.isCatchingUp && pcard == mainGame->dField.hovered_card)
				mainGame->dField.hovered_card = nullptr;
			delete pcard;
		} else {
			if (!(previous.location & LOCATION_OVERLAY) && !(current.location & LOCATION_OVERLAY)) {
				ClientCard* pcard = mainGame->dField.GetCard(previous.controler, previous.location, previous.sequence);
				if (pcard->code != code && (code != 0 || current.location == LOCATION_EXTRA))
					pcard->SetCode(code);
				pcard->cHint = 0;
				pcard->chValue = 0;
				if((previous.location & LOCATION_ONFIELD) && (current.location != previous.location))
					pcard->counters.clear();
				if(current.location != previous.location) {
					pcard->ClearTarget();
					if(pcard->equipTarget) {
						pcard->equipTarget->is_showequip = false;
						pcard->equipTarget->equipped.erase(pcard);
						pcard->equipTarget = 0;
					}
				}
				pcard->is_hovered = false;
				pcard->is_showequip = false;
				pcard->is_showtarget = false;
				pcard->is_showchaintarget = false;
				mainGame->dField.RemoveCard(previous.controler, previous.location, previous.sequence);
				pcard->position = current.position;
				mainGame->dField.AddCard(pcard, current.controler, current.location, current.sequence);
				if(!mainGame->dInfo.isCatchingUp) {
					if (previous.location == current.location && previous.controler == current.controler && (current.location & (LOCATION_DECK | LOCATION_GRAVE | LOCATION_REMOVED | LOCATION_EXTRA))) {
						constexpr float milliseconds = 5.0f * 1000.0f / 60.0f;
						pcard->dPos.set(-1.5f / milliseconds, 0, 0);
						pcard->dRot.set(0, 0, 0);
						if (previous.controler == 1) pcard->dPos.X *= -0.1f;
						pcard->is_moving = true;
						pcard->aniFrame = milliseconds;
						mainGame->WaitFrameSignal(5, lock);
						mainGame->dField.MoveCard(pcard, 5);
						mainGame->WaitFrameSignal(5, lock);
					} else {
						if (current.location == LOCATION_MZONE && pcard->overlayed.size() > 0) {
							for (const auto& ocard : pcard->overlayed)
								mainGame->dField.MoveCard(ocard, 10);
							mainGame->WaitFrameSignal(10, lock);
						}
						if (current.location == LOCATION_HAND) {
							for (const auto& hcard : mainGame->dField.hand[current.controler])
								mainGame->dField.MoveCard(hcard, 10);
						} else
							mainGame->dField.MoveCard(pcard, 10);
						if(previous.location == LOCATION_HAND) {
							for(const auto& hcard : mainGame->dField.hand[previous.controler])
								mainGame->dField.MoveCard(hcard, 10);
						}
						mainGame->WaitFrameSignal(5, lock);
					}
				}
			} else if (!(previous.location & LOCATION_OVERLAY)) {
				ClientCard* pcard = mainGame->dField.GetCard(previous.controler, previous.location, previous.sequence);
				if (code != 0 && pcard->code != code)
					pcard->SetCode(code);
				pcard->counters.clear();
				pcard->ClearTarget();
				pcard->is_showtarget = false;
				pcard->is_showchaintarget = false;
				mainGame->dField.RemoveCard(previous.controler, previous.location, previous.sequence);
				ClientCard* olcard = mainGame->dField.GetCard(current.controler, current.location & (~LOCATION_OVERLAY) & 0xff, current.sequence);
				olcard->overlayed.push_back(pcard);
				mainGame->dField.overlay_cards.insert(pcard);
				pcard->overlayTarget = olcard;
				pcard->location = LOCATION_OVERLAY;
				pcard->sequence = static_cast<uint32_t>(olcard->overlayed.size() - 1);
				if(!mainGame->dInfo.isCatchingUp && (olcard->location & LOCATION_ONFIELD)) {
					mainGame->dField.MoveCard(pcard, 10);
					if(previous.location == LOCATION_HAND)
						for(const auto& hcard : mainGame->dField.hand[previous.controler])
							mainGame->dField.MoveCard(hcard, 10);
					mainGame->WaitFrameSignal(5, lock);
				}
				if(!mainGame->dInfo.isCatchingUp)
					mainGame->WaitFrameSignal(5, lock);
			} else if (!(current.location & LOCATION_OVERLAY)) {
				ClientCard* olcard = mainGame->dField.GetCard(previous.controler, previous.location & (~LOCATION_OVERLAY) & 0xff, previous.sequence);
				ClientCard* pcard = olcard->overlayed[previous.position];
				olcard->overlayed.erase(olcard->overlayed.begin() + pcard->sequence);
				pcard->overlayTarget = 0;
				pcard->position = current.position;
				mainGame->dField.AddCard(pcard, current.controler, current.location, current.sequence);
				mainGame->dField.overlay_cards.erase(pcard);
				for(size_t i = 0; i < olcard->overlayed.size(); ++i) {
					olcard->overlayed[i]->sequence = static_cast<uint32_t>(i);
					if(!mainGame->dInfo.isCatchingUp)
						mainGame->dField.MoveCard(olcard->overlayed[i], 2);
				}
				if(!mainGame->dInfo.isCatchingUp) {
					mainGame->WaitFrameSignal(5, lock);
					mainGame->dField.MoveCard(pcard, 10);
					mainGame->WaitFrameSignal(5, lock);
				}
			} else {
				ClientCard* olcard1 = mainGame->dField.GetCard(previous.controler, previous.location & (~LOCATION_OVERLAY) & 0xff, previous.sequence);
				ClientCard* pcard = olcard1->overlayed[previous.position];
				ClientCard* olcard2 = mainGame->dField.GetCard(current.controler, current.location & (~LOCATION_OVERLAY) & 0xff, current.sequence);
				olcard1->overlayed.erase(olcard1->overlayed.begin() + pcard->sequence);
				olcard2->overlayed.push_back(pcard);
				pcard->sequence = static_cast<uint32_t>(olcard2->overlayed.size() - 1);
				pcard->location = LOCATION_OVERLAY;
				pcard->overlayTarget = olcard2;
				for(size_t i = 0; i < olcard1->overlayed.size(); ++i) {
					olcard1->overlayed[i]->sequence = static_cast<uint32_t>(i);
					if(!mainGame->dInfo.isCatchingUp)
						mainGame->dField.MoveCard(olcard1->overlayed[i], 2);
				}
				if(!mainGame->dInfo.isCatchingUp) {
					mainGame->dField.MoveCard(pcard, 10);
					mainGame->WaitFrameSignal(5, lock);
				}
			}
		}
		return true;
	}
	case MSG_POS_CHANGE: {
		const auto code = BufferIO::Read<uint32_t>(pbuf);
		const auto cc = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto cl = BufferIO::Read<uint8_t>(pbuf);
		const auto cs = BufferIO::Read<uint8_t>(pbuf);
		const auto pp = BufferIO::Read<uint8_t>(pbuf);
		const auto cp = BufferIO::Read<uint8_t>(pbuf);
		auto lock = LockIf();
		ClientCard* pcard = mainGame->dField.GetCard(cc, cl, cs);
		if((pp & POS_FACEUP) && (cp & POS_FACEDOWN)) {
			pcard->counters.clear();
			pcard->ClearTarget();
		}
		if (code != 0 && pcard->code != code)
			pcard->SetCode(code);
		pcard->position = cp;
		if(!mainGame->dInfo.isCatchingUp) {
			event_string = gDataManager->GetSysString(1600).data();
			mainGame->dField.MoveCard(pcard, 10);
			mainGame->WaitFrameSignal(11, lock);
		}
		return true;
	}
	case MSG_SET: {
		Play(SoundManager::SFX::SET);
		/*const auto code = BufferIO::Read<uint32_t>(pbuf);*/
		/*CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);*/
		event_string = gDataManager->GetSysString(1601).data();
		return true;
	}
	case MSG_SWAP: {
		/*const auto code1 = */BufferIO::Read<uint32_t>(pbuf);
		CoreUtils::loc_info info1 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		info1.controler = mainGame->LocalPlayer(info1.controler);
		/*const auto code2 = */BufferIO::Read<uint32_t>(pbuf);
		CoreUtils::loc_info info2 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		info2.controler = mainGame->LocalPlayer(info2.controler);
		event_string = gDataManager->GetSysString(1602).data();
		ClientCard* pc1 = mainGame->dField.GetCard(info1.controler, info1.location, info1.sequence);
		ClientCard* pc2 = mainGame->dField.GetCard(info2.controler, info2.location, info2.sequence);
		auto lock = LockIf();
		mainGame->dField.RemoveCard(info1.controler, info1.location, info1.sequence);
		mainGame->dField.RemoveCard(info2.controler, info2.location, info2.sequence);
		mainGame->dField.AddCard(pc1, info2.controler, info2.location, info2.sequence);
		mainGame->dField.AddCard(pc2, info1.controler, info1.location, info1.sequence);
		if(!mainGame->dInfo.isCatchingUp) {
			mainGame->dField.MoveCard(pc1, 10);
			mainGame->dField.MoveCard(pc2, 10);
			for(size_t i = 0; i < pc1->overlayed.size(); ++i)
				mainGame->dField.MoveCard(pc1->overlayed[i], 10);
			for(size_t i = 0; i < pc2->overlayed.size(); ++i)
				mainGame->dField.MoveCard(pc2->overlayed[i], 10);
			mainGame->WaitFrameSignal(11, lock);
		}
		return true;
	}
	case MSG_FIELD_DISABLED: {
		auto disabled = BufferIO::Read<uint32_t>(pbuf);
		if (!mainGame->dInfo.isFirst)
			disabled = (disabled >> 16) | (disabled << 16);
		mainGame->dField.disabled_field = disabled;
		return true;
	}
	case MSG_SUMMONING: {
		const auto code = BufferIO::Read<uint32_t>(pbuf);
		/*CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);*/
		if(!PlayChant(SoundManager::CHANT::SUMMON, code))
			Play(SoundManager::SFX::SUMMON);
		if(!mainGame->dInfo.isCatchingUp) {
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			event_string = epro::sprintf(gDataManager->GetSysString(1603), gDataManager->GetName(code));
			mainGame->showcardcode = code;
			mainGame->showcarddif = 0;
			mainGame->showcardp = 0;
			mainGame->showcard = 7;
			mainGame->WaitFrameSignal(30, lock);
			mainGame->showcard = 0;
			mainGame->WaitFrameSignal(11, lock);
		}
		return true;
	}
	case MSG_SUMMONED: {
		event_string = gDataManager->GetSysString(1604).data();
		return true;
	}
	case MSG_SPSUMMONING: {
		const auto code = BufferIO::Read<uint32_t>(pbuf);
		/*CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);*/
		if(!PlayChant(SoundManager::CHANT::SUMMON, code))
			Play(SoundManager::SFX::SPECIAL_SUMMON);
		if(!mainGame->dInfo.isCatchingUp) {
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			event_string = epro::sprintf(gDataManager->GetSysString(1605), gDataManager->GetName(code));
			mainGame->showcardcode = code;
			mainGame->showcarddif = 1;
			mainGame->showcard = 5;
			mainGame->WaitFrameSignal(30, lock);
			mainGame->showcard = 0;
			mainGame->WaitFrameSignal(11, lock);
		}
		return true;
	}
	case MSG_SPSUMMONED: {
		event_string = gDataManager->GetSysString(1606).data();
		return true;
	}
	case MSG_FLIPSUMMONING: {
		const auto code = BufferIO::Read<uint32_t>(pbuf);
		CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		info.controler = mainGame->LocalPlayer(info.controler);
		if(!PlayChant(SoundManager::CHANT::SUMMON, code))
			Play(SoundManager::SFX::FLIP);
		ClientCard* pcard = mainGame->dField.GetCard(info.controler, info.location, info.sequence);
		pcard->SetCode(code);
		pcard->position = info.position;
		if(!mainGame->dInfo.isCatchingUp) {
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			event_string = epro::sprintf(gDataManager->GetSysString(1607), gDataManager->GetName(code));
			mainGame->dField.MoveCard(pcard, 10);
			mainGame->WaitFrameSignal(11, lock);
			mainGame->showcardcode = code;
			mainGame->showcarddif = 0;
			mainGame->showcardp = 0;
			mainGame->showcard = 7;
			mainGame->WaitFrameSignal(30, lock);
			mainGame->showcard = 0;
			mainGame->WaitFrameSignal(11, lock);
		}
		return true;
	}
	case MSG_FLIPSUMMONED: {
		event_string = gDataManager->GetSysString(1608).data();
		return true;
	}
	case MSG_CHAINING: {
		const auto code = BufferIO::Read<uint32_t>(pbuf);
		if (!PlayChant(SoundManager::CHANT::ACTIVATE, code))
			Play(SoundManager::SFX::ACTIVATE);
		CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		const auto cc = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto cl = BufferIO::Read<uint8_t>(pbuf);
		const auto cs = CompatRead<uint8_t, uint32_t>(pbuf);
		const auto desc = CompatRead<uint32_t, uint64_t>(pbuf);
		/*const auto ct = */CompatRead<uint8_t, uint32_t>(pbuf);
		ClientCard* pcard = mainGame->dField.GetCard(mainGame->LocalPlayer(info.controler), info.location, info.sequence, info.position);
		auto lock = LockIf();
		if(pcard->code != code || (!pcard->is_public && !mainGame->dInfo.compat_mode)) {
			pcard->is_public = mainGame->dInfo.compat_mode;
			pcard->code = code;
			if(!mainGame->dInfo.isCatchingUp)
				mainGame->dField.MoveCard(pcard, 10);
		}
		if(!mainGame->dInfo.isCatchingUp) {
			mainGame->showcardcode = code;
			mainGame->showcarddif = 0;
			mainGame->showcard = 1;
			pcard->is_highlighting = true;
			if(pcard->location & 0x30) {
				constexpr float milliseconds = 5.0f * 1000.0f / 60.0f;
				float shift = -0.75f / milliseconds;
				if(info.controler == 1) shift *= -1.0f;
				pcard->dPos.set(shift, 0, 0);
				pcard->dRot.set(0, 0, 0);
				pcard->is_moving = true;
				pcard->aniFrame = milliseconds;
				mainGame->WaitFrameSignal(30, lock);
				mainGame->dField.MoveCard(pcard, 5);
			} else
				mainGame->WaitFrameSignal(30, lock);
			pcard->is_highlighting = false;
		}
		mainGame->dField.current_chain.chain_card = pcard;
		mainGame->dField.current_chain.code = code;
		mainGame->dField.current_chain.desc = desc;
		mainGame->dField.current_chain.controler = cc;
		mainGame->dField.current_chain.location = cl;
		mainGame->dField.current_chain.sequence = cs;
		mainGame->dField.current_chain.UpdateDrawCoordinates();
		mainGame->dField.current_chain.solved = false;
		mainGame->dField.current_chain.target.clear();
		if(cl == LOCATION_HAND)
			mainGame->dField.current_chain.chain_pos.X += 0.35f;
		else {
			int chc = 0;
			for(const auto& chain : mainGame->dField.chains) {
				if(cl == LOCATION_GRAVE || cl == LOCATION_REMOVED) {
					if(chain.controler == cc && chain.location == cl)
						chc++;
				} else {
					if(chain.controler == cc && chain.location == cl && chain.sequence == cs)
						chc++;
				}
			}
			mainGame->dField.current_chain.chain_pos.Y += chc * 0.25f;
		}
		return true;
	}
	case MSG_CHAINED: {
		const auto ct = BufferIO::Read<uint8_t>(pbuf);
		auto lock = LockIf();
		event_string = epro::sprintf(gDataManager->GetSysString(1609), gDataManager->GetName(mainGame->dField.current_chain.code));
		mainGame->dField.chains.push_back(mainGame->dField.current_chain);
		if (ct > 1 && !mainGame->dInfo.isCatchingUp)
			mainGame->WaitFrameSignal(20, lock);
		mainGame->dField.last_chain = true;
		return true;
	}
	case MSG_CHAIN_SOLVING: {
		const auto ct = BufferIO::Read<uint8_t>(pbuf);
		auto lock = LockIf();
		if(!mainGame->dInfo.isCatchingUp) {
			if(mainGame->dField.last_chain)
				mainGame->WaitFrameSignal(11, lock);
			for(int i = 0; i < 5; ++i) {
				mainGame->dField.chains[ct - 1].solved = false;
				mainGame->WaitFrameSignal(3, lock);
				mainGame->dField.chains[ct - 1].solved = true;
				mainGame->WaitFrameSignal(3, lock);
			}
		} else {
			mainGame->dField.chains[ct - 1].solved = true;
		}
		mainGame->dField.last_chain = false;
		return true;
	}
	case MSG_CHAIN_SOLVED: {
		/*const auto ct = BufferIO::Read<uint8_t>(pbuf);*/
		return true;
	}
	case MSG_CHAIN_END: {
		for(const auto& chain : mainGame->dField.chains) {
			for(const auto& target : chain.target)
				target->is_showchaintarget = false;
			chain.chain_card->is_showchaintarget = false;
		}
		mainGame->dField.chains.clear();
		return true;
	}
	case MSG_CHAIN_NEGATED:
	case MSG_CHAIN_DISABLED: {
		const auto ct = BufferIO::Read<uint8_t>(pbuf);
		if(!mainGame->dInfo.isCatchingUp) {
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			mainGame->showcardcode = mainGame->dField.chains[ct - 1].code;
			mainGame->showcarddif = 0;
			mainGame->showcard = 3;
			mainGame->WaitFrameSignal(30, lock);
			mainGame->showcard = 0;
		}
		return true;
	}
	case MSG_RANDOM_SELECTED: {
		/*const auto player = */BufferIO::Read<uint8_t>(pbuf);
		const auto count = CompatRead<uint8_t, uint32_t>(pbuf);
		if(mainGame->dInfo.isCatchingUp) {
			return true;
		}
		std::vector<ClientCard*> pcards;
		pcards.resize(count);
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		for (auto& pcard : pcards) {
			CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
			info.controler = mainGame->LocalPlayer(info.controler);
			if ((info.location & LOCATION_OVERLAY) > 0)
				pcard = mainGame->dField.GetCard(info.controler, info.location & (~LOCATION_OVERLAY) & 0xff, info.sequence)->overlayed[info.position];
			else
				pcard = mainGame->dField.GetCard(info.controler, info.location, info.sequence);
			pcard->is_highlighting = true;
		}
		mainGame->WaitFrameSignal(30, lock);
		for(auto& pcard : pcards)
			pcard->is_highlighting = false;
		return true;
	}
	case MSG_CARD_SELECTED:
	case MSG_BECOME_TARGET: {
		if(mainGame->dInfo.isCatchingUp)
			return true;
		const auto count = CompatRead<uint8_t, uint32_t>(pbuf);
		for(uint32_t i = 0; i < count; ++i) {
			CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
			ClientCard* pcard = mainGame->dField.GetCard(mainGame->LocalPlayer(info.controler), info.location, info.sequence);
			std::unique_lock<epro::mutex> lock(mainGame->gMutex);
			pcard->is_highlighting = true;
			if(mainGame->dInfo.curMsg == MSG_BECOME_TARGET)
				mainGame->dField.current_chain.target.insert(pcard);
			if(pcard->location & LOCATION_ONFIELD) {
				for (int j = 0; j < 3; ++j) {
					mainGame->dField.FadeCard(pcard, 5, 5);
					mainGame->WaitFrameSignal(5, lock);
					mainGame->dField.FadeCard(pcard, 255, 5);
					mainGame->WaitFrameSignal(5, lock);
				}
			} else if(pcard->location & 0x30) {
				constexpr float milliseconds = 5.0f * 1000.0f / 60.0f;
				float shift = -0.75f / milliseconds;
				if(info.controler == 1) shift *= -1.0f;
				pcard->dPos.set(shift, 0, 0);
				pcard->dRot.set(0, 0, 0);
				pcard->is_moving = true;
				pcard->aniFrame = milliseconds;
				mainGame->WaitFrameSignal(30, lock);
				mainGame->dField.MoveCard(pcard, 5);
			} else
				mainGame->WaitFrameSignal(30, lock);
			mainGame->AddLog(epro::sprintf(gDataManager->GetSysString((mainGame->dInfo.curMsg == MSG_BECOME_TARGET) ? 1610 : 1680), gDataManager->GetName(pcard->code), gDataManager->FormatLocation(info.location, info.sequence), info.sequence + 1), pcard->code);
			pcard->is_highlighting = false;
		}
		return true;
	}
	case MSG_DRAW: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto count = CompatRead<uint8_t, uint32_t>(pbuf);
		auto lock = LockIf();
		auto& deck = mainGame->dField.deck[player];
		for (auto it = deck.crbegin(), end = it + count; it != end; ++it) {
			auto pcard = *it;
			const auto code = BufferIO::Read<uint32_t>(pbuf);
			if(!mainGame->dInfo.compat_mode) {
				/*uint32_t position =*/BufferIO::Read<uint32_t>(pbuf);
				if(!mainGame->dField.deck_reversed || code)
					pcard->SetCode(code);
			} else if(!mainGame->dField.deck_reversed || code) {
				pcard->SetCode(code & 0x7fffffff);
			}
		}
		for(uint32_t i = 0; i < count; ++i) {
			Play(SoundManager::SFX::DRAW);
			const auto pcard = deck.back();
			deck.pop_back();
			mainGame->dField.AddCard(pcard, player, LOCATION_HAND, 0);
			if(!mainGame->dInfo.isCatchingUp) {
				for(auto& hand_pcard : mainGame->dField.hand[player])
					mainGame->dField.MoveCard(hand_pcard, 10);
				mainGame->WaitFrameSignal(5, lock);
			}
		}
		event_string = epro::sprintf(gDataManager->GetSysString(1611 + player), count);
		return true;
	}
	case MSG_DAMAGE: {
		Play(SoundManager::SFX::DAMAGE);
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto val = BufferIO::Read<uint32_t>(pbuf);
		int final = mainGame->dInfo.lp[player] - val;
		if (final < 0)
			final = 0;
		auto lock = LockIf();
		if(!mainGame->dInfo.isCatchingUp) {
			mainGame->lpd = (mainGame->dInfo.lp[player] - final) / 10;
			event_string = epro::sprintf(gDataManager->GetSysString(1613 + player), val);
			mainGame->lpccolor = 0xff0000;
			mainGame->lpcalpha = 0xff;
			mainGame->lpplayer = player;
			mainGame->lpcstring = epro::format(L"-{}", val);
			mainGame->WaitFrameSignal(30, lock);
			mainGame->lpframe = 10;
			mainGame->WaitFrameSignal(11, lock);
			mainGame->lpcstring = L"";
		}
		mainGame->dInfo.lp[player] = final;
		mainGame->dInfo.strLP[player] = fmt::to_wstring(mainGame->dInfo.lp[player]);
		return true;
	}
	case MSG_RECOVER: {
		Play(SoundManager::SFX::RECOVER);
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto val = BufferIO::Read<uint32_t>(pbuf);
		const int final = mainGame->dInfo.lp[player] + val;
		auto lock = LockIf();
		if(!mainGame->dInfo.isCatchingUp) {
			mainGame->lpd = (mainGame->dInfo.lp[player] - final) / 10;
			event_string = epro::sprintf(gDataManager->GetSysString(1615 + player), val);
			mainGame->lpccolor = 0x00ff00;
			mainGame->lpcalpha = 0xff;
			mainGame->lpplayer = player;
			mainGame->lpcstring = epro::format(L"+{}", val);
			mainGame->WaitFrameSignal(30, lock);
			mainGame->lpframe = 10;
			mainGame->WaitFrameSignal(11, lock);
			mainGame->lpcstring = L"";
		}
		mainGame->dInfo.lp[player] = final;
		mainGame->dInfo.strLP[player] = fmt::to_wstring(mainGame->dInfo.lp[player]);
		return true;
	}
	case MSG_EQUIP: {
		Play(SoundManager::SFX::EQUIP);
		CoreUtils::loc_info info1 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		CoreUtils::loc_info info2 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		ClientCard* pc1 = mainGame->dField.GetCard(mainGame->LocalPlayer(info1.controler), info1.location, info1.sequence);
		ClientCard* pc2 = mainGame->dField.GetCard(mainGame->LocalPlayer(info2.controler), info2.location, info2.sequence);
		auto lock = LockIf();
		if(pc1->equipTarget)
			pc1->equipTarget->equipped.erase(pc1);
		pc1->equipTarget = pc2;
		pc2->equipped.insert(pc1);
		if(!mainGame->dInfo.isCatchingUp) {
			if(pc1->equipTarget) {
				pc1->is_showequip = false;
				pc1->equipTarget->is_showequip = false;
			}
			if(mainGame->dField.hovered_card == pc1)
				pc2->is_showequip = true;
			else if(mainGame->dField.hovered_card == pc2)
				pc1->is_showequip = true;
		}
		return true;
	}
	case MSG_LPUPDATE: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto val = BufferIO::Read<uint32_t>(pbuf);
		auto lock = LockIf();
		if(!mainGame->dInfo.isCatchingUp) {
			mainGame->lpd = (mainGame->dInfo.lp[player] - val) / 10;
			mainGame->lpplayer = player;
			mainGame->lpframe = 10;
			mainGame->WaitFrameSignal(11, lock);
		}
		mainGame->dInfo.lp[player] = val;
		mainGame->dInfo.strLP[player] = fmt::to_wstring(mainGame->dInfo.lp[player]);
		return true;
	}
	case MSG_UNEQUIP: {
		CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		ClientCard* pc = mainGame->dField.GetCard(mainGame->LocalPlayer(info.controler), info.location, info.sequence);
		auto lock = LockIf();
		if(!mainGame->dInfo.isCatchingUp) {
			if(mainGame->dField.hovered_card == pc)
				pc->equipTarget->is_showequip = false;
			else if(mainGame->dField.hovered_card == pc->equipTarget)
				pc->is_showequip = false;
		}
		pc->equipTarget->equipped.erase(pc);
		pc->equipTarget = 0;
		return true;
	}
	case MSG_CARD_TARGET: {
		CoreUtils::loc_info info1 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		CoreUtils::loc_info info2 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		ClientCard* pc1 = mainGame->dField.GetCard(mainGame->LocalPlayer(info1.controler), info1.location, info1.sequence);
		ClientCard* pc2 = mainGame->dField.GetCard(mainGame->LocalPlayer(info2.controler), info2.location, info2.sequence);
		auto lock = LockIf();
		pc1->cardTarget.insert(pc2);
		pc2->ownerTarget.insert(pc1);
		if(!mainGame->dInfo.isCatchingUp) {
			if(mainGame->dField.hovered_card == pc1)
				pc2->is_showtarget = true;
			else if(mainGame->dField.hovered_card == pc2)
				pc1->is_showtarget = true;
		}
		break;
	}
	case MSG_CANCEL_TARGET: {
		CoreUtils::loc_info info1 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		CoreUtils::loc_info info2 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		ClientCard* pc1 = mainGame->dField.GetCard(mainGame->LocalPlayer(info1.controler), info1.location, info1.sequence);
		ClientCard* pc2 = mainGame->dField.GetCard(mainGame->LocalPlayer(info2.controler), info2.location, info2.sequence);
		auto lock = LockIf();
		pc1->cardTarget.erase(pc2);
		pc2->ownerTarget.erase(pc1);
		if(!mainGame->dInfo.isCatchingUp) {
			if(mainGame->dField.hovered_card == pc1)
				pc2->is_showtarget = false;
			else if(mainGame->dField.hovered_card == pc2)
				pc1->is_showtarget = false;
		}
		break;
	}
	case MSG_PAY_LPCOST: {
		Play(SoundManager::SFX::DAMAGE);
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto cost = BufferIO::Read<uint32_t>(pbuf);
		int final = mainGame->dInfo.lp[player] - cost;
		if (final < 0)
			final = 0;
		auto lock = LockIf();
		if(!mainGame->dInfo.isCatchingUp) {
			mainGame->lpd = (mainGame->dInfo.lp[player] - final) / 10;
			mainGame->lpccolor = 0x0000ff;
			mainGame->lpcalpha = 0xff;
			mainGame->lpplayer = player;
			mainGame->lpcstring = epro::format(L"-{}", cost);
			mainGame->WaitFrameSignal(30, lock);
			mainGame->lpframe = 10;
			mainGame->WaitFrameSignal(11, lock);
			mainGame->lpcstring = L"";
		}
		mainGame->dInfo.lp[player] = final;
		mainGame->dInfo.strLP[player] = fmt::to_wstring(mainGame->dInfo.lp[player]);
		return true;
	}
	case MSG_ADD_COUNTER: {
		Play(SoundManager::SFX::COUNTER_ADD);
		const auto type = BufferIO::Read<uint16_t>(pbuf);
		const auto c = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto l = BufferIO::Read<uint8_t>(pbuf);
		const auto s = BufferIO::Read<uint8_t>(pbuf);
		const auto count = BufferIO::Read<uint16_t>(pbuf);
		ClientCard* pc = mainGame->dField.GetCard(c, l, s);
		if (pc->counters.count(type))
			pc->counters[type] += count;
		else pc->counters[type] = count;
		if(mainGame->dInfo.isCatchingUp)
			return true;
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		pc->is_highlighting = true;
		mainGame->stACMessage->setText(epro::format(gDataManager->GetSysString(1617), gDataManager->GetName(pc->code), gDataManager->GetCounterName(type), count).data());
		mainGame->PopupElement(mainGame->wACMessage, 20);
		mainGame->WaitFrameSignal(40, lock);
		pc->is_highlighting = false;
		return true;
	}
	case MSG_REMOVE_COUNTER: {
		Play(SoundManager::SFX::COUNTER_REMOVE);
		const auto type = BufferIO::Read<uint16_t>(pbuf);
		const auto c = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto l = BufferIO::Read<uint8_t>(pbuf);
		const auto s = BufferIO::Read<uint8_t>(pbuf);
		const auto count = BufferIO::Read<uint16_t>(pbuf);
		ClientCard* pc = mainGame->dField.GetCard(c, l, s);
		auto lock = LockIf();
		pc->counters[type] -= count;
		if (pc->counters[type] <= 0)
			pc->counters.erase(type);
		if(mainGame->dInfo.isCatchingUp)
			return true;
		pc->is_highlighting = true;
		mainGame->stACMessage->setText(epro::format(gDataManager->GetSysString(1618), gDataManager->GetName(pc->code), gDataManager->GetCounterName(type), count).data());
		mainGame->PopupElement(mainGame->wACMessage, 20);
		mainGame->WaitFrameSignal(40, lock);
		pc->is_highlighting = false;
		return true;
	}
	case MSG_ATTACK: {
		CoreUtils::loc_info info1 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		info1.controler = mainGame->LocalPlayer(info1.controler);
		mainGame->dField.attacker = mainGame->dField.GetCard(info1.controler, info1.location, info1.sequence);
		if(!PlayChant(SoundManager::CHANT::ATTACK, mainGame->dField.attacker->code))
			Play(SoundManager::SFX::ATTACK);
		if(mainGame->dInfo.isCatchingUp)
			return true;
		CoreUtils::loc_info info2 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		const bool is_direct = info2.location == 0;
		info2.controler = mainGame->LocalPlayer(info2.controler);
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		float sy;
		float xa = mainGame->dField.attacker->curPos.X;
		float ya = mainGame->dField.attacker->curPos.Y;
		float xd, yd;
		if (!is_direct) {
			mainGame->dField.attack_target = mainGame->dField.GetCard(info2.controler, info2.location, info2.sequence);
			event_string = epro::format(gDataManager->GetSysString(1619), gDataManager->GetName(mainGame->dField.attacker->code),
				gDataManager->GetName(mainGame->dField.attack_target->code));
			xd = mainGame->dField.attack_target->curPos.X;
			yd = mainGame->dField.attack_target->curPos.Y;
		} else {
			event_string = epro::format(gDataManager->GetSysString(1620), gDataManager->GetName(mainGame->dField.attacker->code));
			xd = 3.95f;
			yd = (info1.controler == 0) ? -3.5f : 3.5f;
		}
		sy = std::sqrt((xa - xd) * (xa - xd) + (ya - yd) * (ya - yd)) / 2.0f;
		mainGame->atk_t.set((xa + xd) / 2, (ya + yd) / 2, 0);
		mainGame->atk_r.set(0, 0, -std::atan((xd - xa) / (yd - ya)));
		if(ya <= yd)
			mainGame->atk_r.Z += irr::core::PI;
		matManager.GenArrow(sy);
		mainGame->attack_sv = 0.0f;
		mainGame->is_attacking = true;
		mainGame->WaitFrameSignal(40, lock);
		mainGame->is_attacking = false;
		return true;
	}
	case MSG_BATTLE: {
		if(mainGame->dInfo.isCatchingUp)
			return true;
		CoreUtils::loc_info info1 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		info1.controler = mainGame->LocalPlayer(info1.controler);
		const auto aatk = static_cast<int32_t>(BufferIO::Read<uint32_t>(pbuf));
		const auto adef = static_cast<int32_t>(BufferIO::Read<uint32_t>(pbuf));
		/*const auto da = */BufferIO::Read<uint8_t>(pbuf);
		CoreUtils::loc_info info2 = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		info2.controler = mainGame->LocalPlayer(info2.controler);
		const auto datk = static_cast<int32_t>(BufferIO::Read<uint32_t>(pbuf));
		const auto ddef = static_cast<int32_t>(BufferIO::Read<uint32_t>(pbuf));
		/*const auto dd = */BufferIO::Read<uint8_t>(pbuf);
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		ClientCard* pcard = mainGame->dField.GetCard(info1.controler, info1.location, info1.sequence);
		if(aatk != pcard->attack) {
			pcard->attack = aatk;
			pcard->atkstring = fmt::to_wstring(aatk);
		}
		if(adef != pcard->defense) {
			pcard->defense = adef;
			pcard->defstring = fmt::to_wstring(adef);
		}
		if(info2.location) {
			pcard = mainGame->dField.GetCard(info2.controler, info2.location, info2.sequence);
			if(datk != pcard->attack) {
				pcard->attack = datk;
				pcard->atkstring = fmt::to_wstring(datk);
			}
			if(ddef != pcard->defense) {
				pcard->defense = ddef;
				pcard->defstring = fmt::to_wstring(ddef);
			}
		}
		return true;
	}
	case MSG_ATTACK_DISABLED: {
		event_string = epro::sprintf(gDataManager->GetSysString(1621), gDataManager->GetName(mainGame->dField.attacker->code));
		return true;
	}
	case MSG_DAMAGE_STEP_START: {
		return true;
	}
	case MSG_DAMAGE_STEP_END: {
		return true;
	}
	case MSG_MISSED_EFFECT: {
		if(mainGame->dInfo.isCatchingUp)
			return true;
		CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		uint32_t code = BufferIO::Read<uint32_t>(pbuf);
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->AddLog(epro::sprintf(gDataManager->GetSysString(1622), gDataManager->GetName(code)), code);
		return true;
	}
	case MSG_TOSS_COIN: {
		if(mainGame->dInfo.isCatchingUp)
			return true;
		Play(SoundManager::SFX::COIN);
		/*const auto player = */mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto count = BufferIO::Read<uint8_t>(pbuf);
		std::wstring text(gDataManager->GetSysString(1623));
		for (int i = 0; i < count; ++i) {
			bool res = !!BufferIO::Read<uint8_t>(pbuf);
			text += epro::format(L"[{}]", gDataManager->GetSysString(res ? 60 : 61));
		}
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->AddLog(text);
		mainGame->stACMessage->setText(text.data());
		mainGame->PopupElement(mainGame->wACMessage, 20);
		mainGame->WaitFrameSignal(40, lock);
		return true;
	}
	case MSG_TOSS_DICE: {
		if(mainGame->dInfo.isCatchingUp)
			return true;
		Play(SoundManager::SFX::DICE);
		/*const auto player = */mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto count = BufferIO::Read<uint8_t>(pbuf);
		std::wstring text(gDataManager->GetSysString(1624));
		for (int i = 0; i < count; ++i) {
			uint8_t res = BufferIO::Read<uint8_t>(pbuf);
			text += epro::format(L"[{}]", res);
		}
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->AddLog(text);
		mainGame->stACMessage->setText(text.data());
		mainGame->PopupElement(mainGame->wACMessage, 20);
		mainGame->WaitFrameSignal(40, lock);
		return true;
	}
	case MSG_ROCK_PAPER_SCISSORS: {
		if(mainGame->dInfo.isCatchingUp)
			return true;
		/*const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));*/
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->wHand->setVisible(true);
		return false;
	}
	case MSG_HAND_RES: {
		if(mainGame->dInfo.isCatchingUp)
			return true;
		const auto res = BufferIO::Read<uint8_t>(pbuf);
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->stHintMsg->setVisible(false);
		uint8_t res1 = (res & 0x3) - 1;
		uint8_t res2 = ((res >> 2) & 0x3) - 1;
		if(mainGame->dInfo.isFirst)
			mainGame->showcardcode = res1 + (res2 << 16);
		else
			mainGame->showcardcode = res2 + (res1 << 16);
		mainGame->showcarddif = 50;
		mainGame->showcardp = 0;
		mainGame->showcard = 100;
		mainGame->WaitFrameSignal(60, lock);
		return false;
	}
	case MSG_ANNOUNCE_RACE: {
		/*const auto player = */mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		mainGame->dField.announce_count = BufferIO::Read<uint8_t>(pbuf);
		const auto available = BufferIO::Read<uint64_t>(pbuf);
		uint64_t filter = 0x1;
		for(int i = 0; i < 25; ++i, filter <<= 1) {
			mainGame->chkRace[i]->setChecked(false);
			if(filter & available)
				mainGame->chkRace[i]->setVisible(true);
			else mainGame->chkRace[i]->setVisible(false);
		}
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->wANRace->setText(gDataManager->GetDesc(select_hint ? select_hint : 563, mainGame->dInfo.compat_mode).data());
		mainGame->PopupElement(mainGame->wANRace);
		select_hint = 0;
		return false;
	}
	case MSG_ANNOUNCE_ATTRIB: {
		/*const auto player = */mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		mainGame->dField.announce_count = BufferIO::Read<uint8_t>(pbuf);
		const auto available = BufferIO::Read<uint32_t>(pbuf);
		for(int i = 0, filter = 0x1; i < 7; ++i, filter <<= 1) {
			mainGame->chkAttribute[i]->setChecked(false);
			if(filter & available)
				mainGame->chkAttribute[i]->setVisible(true);
			else mainGame->chkAttribute[i]->setVisible(false);
		}
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->wANAttribute->setText(gDataManager->GetDesc(select_hint ? select_hint : 562, mainGame->dInfo.compat_mode).data());
		mainGame->PopupElement(mainGame->wANAttribute);
		select_hint = 0;
		return false;
	}
	case MSG_ANNOUNCE_CARD: {
		/*const auto player = */mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto count = BufferIO::Read<uint8_t>(pbuf);
		mainGame->dField.declare_opcodes.clear();
		for (int i = 0; i < count; ++i)
			mainGame->dField.declare_opcodes.push_back(CompatRead<uint32_t, uint64_t>(pbuf));
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->ebANCard->setText(L"");
		mainGame->wANCard->setText(gDataManager->GetDesc(select_hint ? select_hint : 564, mainGame->dInfo.compat_mode).data());
		mainGame->dField.UpdateDeclarableList();
		mainGame->PopupElement(mainGame->wANCard);
		select_hint = 0;
		return false;
	}
	case MSG_ANNOUNCE_NUMBER: {
		/*const auto player = */mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto count = BufferIO::Read<uint8_t>(pbuf);
		std::unique_lock<epro::mutex> lock(mainGame->gMutex);
		mainGame->cbANNumber->clear();
		for (int i = 0; i < count; ++i) {
			uint32_t value = (uint32_t)((CompatRead<uint32_t, uint64_t>(pbuf)) & 0xffffffff);
			mainGame->cbANNumber->addItem(epro::format(L" {}", value).data(), value);
		}
		mainGame->cbANNumber->setSelected(0);
		mainGame->wANNumber->setText(gDataManager->GetDesc(select_hint ? select_hint : 565, mainGame->dInfo.compat_mode).data());
		mainGame->PopupElement(mainGame->wANNumber);
		select_hint = 0;
		return false;
	}
	case MSG_CARD_HINT: {
		CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
		const auto chtype = BufferIO::Read<uint8_t>(pbuf);
		const auto value = CompatRead<uint32_t, uint64_t>(pbuf);
		ClientCard* pcard = mainGame->dField.GetCard(mainGame->LocalPlayer(info.controler), info.location, info.sequence);
		if(!pcard)
			return true;
		if(chtype == CHINT_DESC_ADD) {
			pcard->desc_hints[value]++;
		} else if(chtype == CHINT_DESC_REMOVE) {
			pcard->desc_hints[value]--;
			if(pcard->desc_hints[value] <= 0)
				pcard->desc_hints.erase(value);
		} else {
			pcard->cHint = chtype;
			pcard->chValue = value;
			if(chtype == CHINT_TURN) {
				if(value == 0)
					return true;
				if(mainGame->dInfo.isCatchingUp)
					return true;
				if(pcard->location & LOCATION_ONFIELD)
					pcard->is_highlighting = true;
				std::unique_lock<epro::mutex> lock(mainGame->gMutex);
				mainGame->showcardcode = pcard->code;
				mainGame->showcarddif = 0;
				mainGame->showcardp = (value & 0xffff) - 1;
				mainGame->showcard = 6;
				mainGame->WaitFrameSignal(30, lock);
				pcard->is_highlighting = false;
				mainGame->showcard = 0;
			}
		}
		return true;
	}
	case MSG_PLAYER_HINT: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto chtype = BufferIO::Read<uint8_t>(pbuf);
		const auto value = CompatRead<uint32_t, uint64_t>(pbuf);
		auto& player_desc_hints = mainGame->dField.player_desc_hints[player];
		if(chtype == PHINT_DESC_ADD) {
			player_desc_hints[value]++;
		} else if(chtype == PHINT_DESC_REMOVE) {
			player_desc_hints[value]--;
			if(player_desc_hints[value] <= 0)
				player_desc_hints.erase(value);
		}
		return true;
	}
	case MSG_MATCH_KILL: {
		match_kill = BufferIO::Read<uint32_t>(pbuf);
		return true;
	}
	case MSG_REMOVE_CARDS: {
		const auto count = BufferIO::Read<uint32_t>(pbuf);
		if(count > 0) {
			std::vector<ClientCard*> cards;
			cards.resize(count);
			for(auto& pcard : cards) {
				CoreUtils::loc_info loc = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
				loc.controler = mainGame->LocalPlayer(loc.controler);
				if(loc.location & LOCATION_OVERLAY) {
					auto olcard = mainGame->dField.GetCard(loc.controler, (loc.location & (~LOCATION_OVERLAY)) & 0xff, loc.sequence);
					pcard = olcard->overlayed[loc.position];
				} else
					pcard = mainGame->dField.GetCard(loc.controler, loc.location, loc.sequence);
			}
			auto lock = LockIf();
			if(!mainGame->dInfo.isCatchingUp) {
				for(auto& pcard : cards)
					mainGame->dField.FadeCard(pcard, 5, 5);
				mainGame->WaitFrameSignal(5, lock);
			}
			for(auto& pcard : cards) {
				if(pcard == mainGame->dField.hovered_card)
					mainGame->dField.hovered_card = 0;
				if(pcard->location & LOCATION_OVERLAY) {
					pcard->overlayTarget->overlayed.erase(pcard->overlayTarget->overlayed.begin() + pcard->sequence);
					mainGame->dField.overlay_cards.erase(pcard);
					for(size_t j = 0; j < pcard->overlayTarget->overlayed.size(); ++j)
						pcard->overlayTarget->overlayed[j]->sequence = static_cast<uint32_t>(j);
					pcard->overlayTarget = 0;
				} else
					mainGame->dField.RemoveCard(pcard->controler, pcard->location, pcard->sequence);
				delete pcard;
			}
		}
		return true;
	}
	case MSG_TAG_SWAP: {
		const auto player = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
		const auto mcount = CompatRead<uint8_t, uint32_t>(pbuf);
		const auto ecount = CompatRead<uint8_t, uint32_t>(pbuf);
		const auto pcount = CompatRead<uint8_t, uint32_t>(pbuf);
		const auto hcount = CompatRead<uint8_t, uint32_t>(pbuf);
		const auto topcode = BufferIO::Read<uint32_t>(pbuf);
		auto MatchPile = [player,&pcount](auto& pile, uint32_t count, uint8_t location) {
			if(pile.size() > count) {
				for(auto cit = pile.begin() + count; cit != pile.end(); cit++)
					delete *cit;
				pile.resize(count);
			} else {
				while(pile.size() < count) {
					ClientCard* ccard = new ClientCard{};
					ccard->controler = player;
					ccard->location = location;
					ccard->sequence = static_cast<uint32_t>(pile.size());
					ccard->position = POS_FACEDOWN;
					pile.push_back(ccard);
				}
			}
			if(location == LOCATION_EXTRA && pcount) {
				auto i = pcount;
				for(auto it = pile.rbegin(); i > 0 && it != pile.rend(); it++, i--) {
					(*it)->position = POS_FACEUP;
				}
			}
		};
		auto lock = LockIf();
		if(!mainGame->dInfo.isCatchingUp) {
			constexpr float milliseconds = 5.0f * 1000.0f / 60.0f;
			for(auto* list : { &mainGame->dField.deck[player] , &mainGame->dField.hand[player] , &mainGame->dField.extra[player] }) {
				for(const auto& pcard : *list) {
					if(player == 0) pcard->dPos.Y = 2.0f / milliseconds;
					else pcard->dPos.Y = -3.0f / milliseconds;
					pcard->dRot.set(0, 0, 0);
					pcard->is_moving = true;
					pcard->aniFrame = milliseconds;
				}
			}
			mainGame->WaitFrameSignal(5, lock);
		}
		MatchPile(mainGame->dField.deck[player], mcount, LOCATION_DECK);
		MatchPile(mainGame->dField.hand[player], hcount, LOCATION_HAND);
		MatchPile(mainGame->dField.extra[player], ecount, LOCATION_EXTRA);
		mainGame->dField.extra_p_count[player] = pcount;
		//
		if(!mainGame->dInfo.isCatchingUp) {
			for (const auto& pcard : mainGame->dField.deck[player]) {
				pcard->UpdateDrawCoordinates();
				if(player == 0) pcard->curPos.Y += 2.0f;
				else pcard->curPos.Y -= 3.0f;
				mainGame->dField.MoveCard(pcard, 5);
			}
			if(mainGame->dField.deck[player].size())
				mainGame->dField.deck[player].back()->code = topcode;
			for(auto* list : { &mainGame->dField.hand[player] , &mainGame->dField.extra[player] }) {
				for(const auto& pcard : *list) {
					if(!mainGame->dInfo.compat_mode) {
						pcard->code = BufferIO::Read<uint32_t>(pbuf);
						pcard->position = BufferIO::Read<uint32_t>(pbuf);
					} else {
						const auto flag = BufferIO::Read<uint32_t>(pbuf);
						pcard->code = flag & 0x7fffffff;
						pcard->position = flag & 0x80000000 ? POS_FACEUP : POS_FACEDOWN;
					}
					pcard->UpdateDrawCoordinates();
					if(player == 0) pcard->curPos.Y += 2.0f;
					else pcard->curPos.Y -= 3.0f;
					mainGame->dField.MoveCard(pcard, 5);
				}
			}
			mainGame->WaitFrameSignal(5, lock);
		}
		mainGame->dInfo.current_player[player] = (mainGame->dInfo.current_player[player] + 1) % ((player == 0 && mainGame->dInfo.isFirst) ? mainGame->dInfo.team1 : mainGame->dInfo.team2);
		break;
	}
	case MSG_RELOAD_FIELD: {
		auto lock = LockIf();
		mainGame->dField.Clear();
		if(mainGame->dInfo.compat_mode) {
			uint8_t field = BufferIO::Read<uint8_t>(pbuf) & 0xf;
			mainGame->dInfo.duel_field = field;
			switch(field) {
				case 1: mainGame->dInfo.duel_params = DUEL_MODE_MR1; break;
				case 2: mainGame->dInfo.duel_params = DUEL_MODE_MR2; break;
				case 3: mainGame->dInfo.duel_params = DUEL_MODE_MR3; break;
				case 4: mainGame->dInfo.duel_params = DUEL_MODE_MR4; break;
				default:
				case 5: mainGame->dInfo.duel_params = DUEL_MODE_MR5; break;
			}
		} else {
			uint32_t opts = BufferIO::Read<uint32_t>(pbuf);
			mainGame->dInfo.duel_field = mainGame->GetMasterRule(opts);
			mainGame->dInfo.duel_params = opts;
		}
		matManager.SetActiveVertices(mainGame->dInfo.HasFieldFlag(DUEL_3_COLUMNS_FIELD),
									 !mainGame->dInfo.HasFieldFlag(DUEL_SEPARATE_PZONE));
		mainGame->SetPhaseButtons();
		for(int i = 0; i < 2; ++i) {
			int p = mainGame->LocalPlayer(i);
			mainGame->dInfo.lp[p] = BufferIO::Read<uint32_t>(pbuf);
			mainGame->dInfo.strLP[p] = fmt::to_wstring(mainGame->dInfo.lp[p]);
			for(int seq = 0; seq < 7; ++seq) {
				const auto is_zone_used = !!BufferIO::Read<uint8_t>(pbuf);
				if(!is_zone_used)
					continue;
				ClientCard* ccard = new ClientCard{};
				mainGame->dField.AddCard(ccard, p, LOCATION_MZONE, seq);
				ccard->position = BufferIO::Read<uint8_t>(pbuf);
				const auto xyz_mat_count = CompatRead<uint8_t, uint32_t>(pbuf);
				for(uint32_t xyz = 0; xyz < xyz_mat_count; ++xyz) {
					ClientCard* xcard = new ClientCard{};
					ccard->overlayed.push_back(xcard);
					mainGame->dField.overlay_cards.insert(xcard);
					xcard->overlayTarget = ccard;
					xcard->location = LOCATION_OVERLAY;
					xcard->sequence = static_cast<uint32_t>(ccard->overlayed.size() - 1);
					xcard->owner = p;
					xcard->controler = p;
				}
			}
			for(int seq = 0; seq < 8; ++seq) {
				const auto is_zone_used = !!BufferIO::Read<uint8_t>(pbuf);
				if(!is_zone_used)
					continue;
				ClientCard* ccard = new ClientCard{};
				mainGame->dField.AddCard(ccard, p, LOCATION_SZONE, seq);
				ccard->position = BufferIO::Read<uint8_t>(pbuf);
				if(mainGame->dInfo.compat_mode)
					continue;
				const auto xyz_mat_count = BufferIO::Read<uint32_t>(pbuf);
				for(uint32_t xyz = 0; xyz < xyz_mat_count; ++xyz) {
					ClientCard* xcard = new ClientCard{};
					ccard->overlayed.push_back(xcard);
					mainGame->dField.overlay_cards.insert(xcard);
					xcard->overlayTarget = ccard;
					xcard->location = LOCATION_OVERLAY;
					xcard->sequence = static_cast<uint32_t>(ccard->overlayed.size() - 1);
					xcard->owner = p;
					xcard->controler = p;
				}
			}
			auto push_range = [&pbuf,p](uint8_t location) {
				const auto val = CompatRead<uint8_t, uint32_t>(pbuf);
				for(uint32_t seq = 0; seq < val; ++seq)
					mainGame->dField.AddCard(new ClientCard{}, p, location, seq);
			};
			push_range(LOCATION_DECK);
			push_range(LOCATION_HAND);
			push_range(LOCATION_GRAVE);
			push_range(LOCATION_REMOVED);
			push_range(LOCATION_EXTRA);
			mainGame->dField.extra_p_count[p] = CompatRead<uint8_t, uint32_t>(pbuf);
		}
		mainGame->dInfo.startlp = std::max(mainGame->dInfo.lp[0], mainGame->dInfo.lp[1]);
		mainGame->dField.RefreshAllCards();
		const auto solving_chains = CompatRead<uint8_t, uint32_t>(pbuf);
		if(solving_chains > 0) {
			for(uint32_t i = 0; i < solving_chains; ++i) {
				uint32_t code = BufferIO::Read<uint32_t>(pbuf);
				CoreUtils::loc_info info = CoreUtils::ReadLocInfo(pbuf, mainGame->dInfo.compat_mode);
				info.controler = mainGame->LocalPlayer(info.controler);
				uint8_t cc = mainGame->LocalPlayer(BufferIO::Read<uint8_t>(pbuf));
				uint8_t cl = BufferIO::Read<uint8_t>(pbuf);
				uint32_t cs = BufferIO::Read<uint32_t>(pbuf);
				uint64_t desc = CompatRead<uint32_t, uint64_t>(pbuf);
				ClientCard* pcard = mainGame->dField.GetCard(info.controler, info.location, info.sequence, info.position);
				mainGame->dField.current_chain.chain_card = pcard;
				mainGame->dField.current_chain.code = code;
				mainGame->dField.current_chain.desc = desc;
				mainGame->dField.current_chain.controler = cc;
				mainGame->dField.current_chain.location = cl;
				mainGame->dField.current_chain.sequence = cs;
				mainGame->dField.current_chain.UpdateDrawCoordinates();
				mainGame->dField.current_chain.solved = false;
				int chc = 0;
				for(const auto& chain : mainGame->dField.chains) {
					if(cl == LOCATION_GRAVE || cl == LOCATION_REMOVED) {
						if(chain.controler == cc && chain.location == cl)
							chc++;
					} else {
						if(chain.controler == cc && chain.location == cl && chain.sequence == cs)
							chc++;
					}
				}
				if(cl == LOCATION_HAND)
					mainGame->dField.current_chain.chain_pos.X += 0.35f;
				else
					mainGame->dField.current_chain.chain_pos.Y += chc * 0.25f;
				mainGame->dField.chains.push_back(mainGame->dField.current_chain);
			}
			event_string = epro::sprintf(gDataManager->GetSysString(1609), gDataManager->GetName(mainGame->dField.current_chain.code));
			mainGame->dField.last_chain = true;
		}
		break;
	}
	}
	return true;
}
void DuelClient::SwapField() {
	if(!analyzeMutex.try_lock())
		is_swapping = !is_swapping;
	else {
		std::lock_guard<epro::mutex> lock(mainGame->gMutex);
		mainGame->dField.ReplaySwap();
		analyzeMutex.unlock();
	}
}

void DuelClient::SendResponse() {
	if(answered.exchange(true))
		return;
	auto& msg = mainGame->dInfo.curMsg;
	switch(msg) {
	case MSG_SELECT_BATTLECMD:
	case MSG_SELECT_IDLECMD: {
		for(auto& pcard : mainGame->dField.limbo_temp)
			delete pcard;
		mainGame->dField.limbo_temp.clear();
		mainGame->dField.ClearCommandFlag();
		if(gGameConfig->alternative_phase_layout) {
			mainGame->btnBP->setEnabled(false);
			mainGame->btnM2->setEnabled(false);
			mainGame->btnEP->setEnabled(false);
		} else {
			if(msg == MSG_SELECT_BATTLECMD) {
				mainGame->btnM2->setVisible(false);
			} else {
				mainGame->btnBP->setVisible(false);
			}
			mainGame->btnEP->setVisible(false);
		}
		mainGame->btnShuffle->setVisible(false);
		break;
	}
	case MSG_SELECT_CARD:
	case MSG_SELECT_UNSELECT_CARD:
	case MSG_SELECT_CHAIN:
	case MSG_CONFIRM_CARDS: {
		for(auto& pcard : mainGame->dField.limbo_temp)
			delete pcard;
		mainGame->dField.limbo_temp.clear();
		if(msg == MSG_SELECT_CHAIN)
			mainGame->dField.ClearChainSelect();
		if(msg != MSG_SELECT_CARD && msg != MSG_SELECT_UNSELECT_CARD)
			break;
	}
	/*fallthrough*/
	case MSG_SELECT_TRIBUTE:
	case MSG_SELECT_COUNTER: {
		mainGame->dField.ClearSelect();
		break;
	}
	case MSG_SELECT_SUM: {
		for(auto& pcard : mainGame->dField.must_select_cards)
			pcard->is_selected = false;
		for(auto& pcard : mainGame->dField.selectsum_all) {
			pcard->is_selectable = false;
			pcard->is_selected = false;
		}
		mainGame->dField.must_select_cards.clear();
		mainGame->dField.selectsum_all.clear();
		break;
	}
	}
	if(mainGame->dInfo.isSingleMode) {
		SingleMode::SetResponse(response_buf.data(), response_buf.size());
		SingleMode::singleSignal.Set();
	} else if (!mainGame->dInfo.isReplay) {
		if(replay_stream.size())
			replay_stream.pop_back();
		/*if(mainGame->dInfo.time_player == 0)
			SendPacketToServer(CTOS_TIME_CONFIRM);*/
		mainGame->dInfo.time_player = 2;
		SendBufferToServer(CTOS_RESPONSE, response_buf.data(), response_buf.size());
	}
}

static std::vector<uint32_t> getAddresses() {
	std::vector<uint32_t> addresses;
#if EDOPRO_ANDROID
	return porting::getLocalIP();
#elif EDOPRO_WINDOWS
	char hname[256];
	gethostname(hname, 256);
	evutil_addrinfo hints;
	evutil_addrinfo* res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	if(evutil_getaddrinfo(hname, nullptr, &hints, &res) != 0)
		return {};
	for(auto* ptr = res; ptr != nullptr && addresses.size() < 8; ptr = ptr->ai_next) {
		if(ptr->ai_family == PF_INET) {
			auto addr_in = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
			if(addr_in->sin_addr.s_addr != 0)
				addresses.emplace_back(addr_in->sin_addr.s_addr);
		}
	}
	evutil_freeaddrinfo(res);
#else
	ifaddrs* allInterfaces;
	if(getifaddrs(&allInterfaces) != 0)
		return {};
	for(auto* interface = allInterfaces; interface != nullptr && addresses.size() < 8; interface = interface->ifa_next) {
		auto flags = interface->ifa_flags;
		sockaddr* addr = interface->ifa_addr;
		if((flags & (IFF_UP | IFF_RUNNING | IFF_LOOPBACK)) != (IFF_UP | IFF_RUNNING))
			continue;
		if(addr->sa_family == AF_INET) {
			auto addr_in = reinterpret_cast<sockaddr_in*>(addr);
			if(addr_in->sin_addr.s_addr != 0)
				addresses.emplace_back(addr_in->sin_addr.s_addr);
		}
	}
	freeifaddrs(allInterfaces);
#endif
	return addresses;
}

void DuelClient::BeginRefreshHost() {
	if(is_refreshing)
		return;
	is_refreshing = true;
	mainGame->btnLanRefresh->setEnabled(false);
	mainGame->lstHostList->clear();
	remotes.clear();
	hosts.clear();
	const auto addresses = getAddresses();
	if(addresses.empty()) {
		mainGame->btnLanRefresh->setEnabled(true);
		is_refreshing = false;
		return;
	}
	event_base* broadev = event_base_new();
	if(!broadev)
		return;
	evutil_socket_t reply = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(reply == EVUTIL_INVALID_SOCKET) {
		event_base_free(broadev);
		return;
	}
	sockaddr_in reply_addr;
	memset(&reply_addr, 0, sizeof(reply_addr));
	reply_addr.sin_family = AF_INET;
	reply_addr.sin_port = htons(7921);
	reply_addr.sin_addr.s_addr = 0;
	if(bind(reply, reinterpret_cast<sockaddr*>(&reply_addr), sizeof(reply_addr)) == -1) {
		evutil_closesocket(reply);
		event_base_free(broadev);
		mainGame->btnLanRefresh->setEnabled(true);
		is_refreshing = false;
		return;
	}
	timeval timeout = { 3, 0 };
	resp_event = event_new(broadev, reply, EV_TIMEOUT | EV_READ | EV_PERSIST, BroadcastReply, broadev);
	event_add(resp_event, &timeout);
	epro::thread(RefreshThread, broadev).detach();
	//send request
	sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = htons(7922);
	sockaddr_in sockTo;
	sockTo.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	sockTo.sin_family = AF_INET;
	sockTo.sin_port = htons(7920);
	HostRequest hReq;
	hReq.identifier = NETWORK_CLIENT_ID;
	for(const auto& address : addresses) {
		local.sin_addr.s_addr = address;
		evutil_socket_t sSend = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if(sSend == EVUTIL_INVALID_SOCKET)
			continue;
		int opt = 1;
		setsockopt(sSend, SOL_SOCKET, SO_BROADCAST, (const char*)&opt,
				   (ev_socklen_t)sizeof(opt));
		if(bind(sSend, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == -1) {
			evutil_closesocket(sSend);
			continue;
		}
		sendto(sSend, reinterpret_cast<const char*>(&hReq), sizeof(HostRequest), 0,
			   reinterpret_cast<sockaddr*>(&sockTo), sizeof(sockTo));
		evutil_closesocket(sSend);
	}
}
int DuelClient::RefreshThread(event_base* broadev) {
	Utils::SetThreadName("RefreshThread");
	event_base_dispatch(broadev);
	evutil_socket_t fd;
	event_get_assignment(resp_event, 0, &fd, 0, 0, 0);
	evutil_closesocket(fd);
	event_free(resp_event);
	event_base_free(broadev);
	is_refreshing = false;
	return 0;
}
void DuelClient::BroadcastReply(evutil_socket_t fd, short events, void* arg) {
	if(events & EV_TIMEOUT) {
		evutil_closesocket(fd);
		event_base_loopbreak((event_base*)arg);
		if(!is_closing)
			mainGame->btnLanRefresh->setEnabled(true);
	} else if(events & EV_READ) {
		sockaddr_in bc_addr;
		ev_socklen_t sz = sizeof(sockaddr_in);
		HostPacket packet{};
		(void)recvfrom(fd, reinterpret_cast<char*>(&packet), sizeof(HostPacket), 0, (sockaddr*)&bc_addr, &sz);
		uint32_t ipaddr = bc_addr.sin_addr.s_addr;
		if(is_closing)
			return;
		if(packet.identifier != NETWORK_SERVER_ID)
			return;
		const auto remote = std::make_pair(ipaddr, packet.port);
		if(remotes.find(remote) == remotes.end()) {
			std::lock_guard<epro::mutex> lock(mainGame->gMutex);
			remotes.insert(remote);
			packet.ipaddr = ipaddr;
			hosts.push_back(packet);
			const auto is_compact_mode = packet.host.handshake != SERVER_HANDSHAKE;
			int rule = packet.host.duel_rule;
			auto GetRuleString = [&]()-> std::wstring {
				if(!is_compact_mode) {
					auto duel_flag = (((uint64_t)packet.host.duel_flag_low) | ((uint64_t)packet.host.duel_flag_high) << 32);
					mainGame->GetMasterRule(duel_flag & ~(DUEL_RELAY | DUEL_TCG_SEGOC_NONPUBLIC | DUEL_PSEUDO_SHUFFLE), packet.host.forbiddentypes, &rule);
					if(rule == 6) {
						if(duel_flag == DUEL_MODE_GOAT)
							return L"GOAT";
						if(duel_flag == DUEL_MODE_RUSH)
							return L"Rush";
						if(duel_flag == DUEL_MODE_SPEED)
							return L"Speed";
						return L"Custom MR";
					}
				}
				return epro::format(L"MR {}", (rule == 0) ? 3 : rule);
			};
			auto GetIsCustom = [&packet,&rule, is_compact_mode] {
				static constexpr DeckSizes normal_sizes{ {40,60}, {0,15}, {0,15} };
				if(packet.host.draw_count == 1 && packet.host.start_hand == 5 && packet.host.start_lp == 8000
				   && !packet.host.no_check_deck_content && !packet.host.no_shuffle_deck
				   && (packet.host.duel_flag_low & DUEL_PSEUDO_SHUFFLE) == 0
				   && rule == DEFAULT_DUEL_RULE && packet.host.extra_rules == 0
				   && (is_compact_mode || (packet.host.version.client.major < 40) || packet.host.sizes == normal_sizes))
					return gDataManager->GetSysString(1280);
				return gDataManager->GetSysString(1281);
			};
			auto FormatVersion = [&packet, is_compact_mode] {
				if(is_compact_mode)
					return epro::format(L"Fluo: {:X}.0{:X}.{:X}", packet.version >> 12, (packet.version >> 4) & 0xff, packet.version & 0xf);
				const auto& version = packet.host.version;
				return epro::format(L"{}.{}", version.client.major, version.client.minor);
			};
			wchar_t gamename[20];
			BufferIO::DecodeUTF16(packet.name, gamename, 20);
			auto hoststr = epro::format(L"[{}][{}][{}][{}][{}][{}]{}",
									   gdeckManager->GetLFListName(packet.host.lflist),
									   gDataManager->GetSysString(packet.host.rule + 1900),
									   gDataManager->GetSysString(packet.host.mode + 1244),
									   FormatVersion(),
									   GetRuleString(),
									   GetIsCustom(),
									   gamename);
			mainGame->lstHostList->addItem(hoststr.data());
		}
	}
}
void DuelClient::ReplayPrompt(bool local_stream) {
	if(local_stream) {
		auto replay_header = ExtendedReplayHeader::CreateDefaultHeader(REPLAY_YRPX, static_cast<uint32_t>(time(0)));
		if(mainGame->dInfo.compat_mode)
			replay_header.base.flag &= ~REPLAY_LUA64;
		last_replay.BeginRecord(false);
		last_replay.WriteHeader(replay_header);
		last_replay.Write<uint32_t>(static_cast<uint32_t>(mainGame->dInfo.selfnames.size()), false);
		for(auto& name : mainGame->dInfo.selfnames) {
			last_replay.WriteData(name.data(), 40, false);
		}
		last_replay.Write<uint32_t>(static_cast<uint32_t>(mainGame->dInfo.opponames.size()), false);
		for(auto& name : mainGame->dInfo.opponames) {
			last_replay.WriteData(name.data(), 40, false);
		}
		last_replay.Write<uint64_t>(mainGame->dInfo.duel_params);
		last_replay.WriteStream(replay_stream);
		last_replay.EndRecord();
	}
	replay_stream.clear();
	std::unique_lock<epro::mutex> lock(mainGame->gMutex);
	mainGame->wPhase->setVisible(false);
	if(mainGame->dInfo.player_type < 7)
		mainGame->btnLeaveGame->setVisible(false);
	mainGame->btnChainIgnore->setVisible(false);
	mainGame->btnChainAlways->setVisible(false);
	mainGame->btnChainWhenAvail->setVisible(false);
	mainGame->btnCancelOrFinish->setVisible(false);
	auto now = std::time(nullptr);
	mainGame->PopupSaveWindow(gDataManager->GetSysString(1340), epro::format(L"{:%Y-%m-%d %H-%M-%S}", fmt::localtime(now)), gDataManager->GetSysString(1342));
	mainGame->replaySignal.Wait(lock);
	if(mainGame->saveReplay || !is_local_host) {
		if(mainGame->saveReplay)
			last_replay.SaveReplay(Utils::ToPathString(mainGame->ebFileSaveName->getText()));
		else last_replay.SaveReplay(EPRO_TEXT("_LastReplay"));
	}
}
}
