#include <nlohmann/json.hpp>
#include <IGUITable.h>
#include <IGUIEditBox.h>
#include <IGUIComboBox.h>
#include <IGUIButton.h>
#include <IGUICheckBox.h>
#include <IGUIWindow.h>
#include <ICursorControl.h>
#include "server_lobby.h"
#include <curl/curl.h>
#include "utils.h"
#include "data_manager.h"
#include "game.h"
#include "duelclient.h"
#include "logging.h"
#include "utils_gui.h"
#include "custom_skin_enum.h"
#include "game_config.h"
#include "address.h"

namespace ygo {

std::vector<RoomInfo> ServerLobby::roomsVector;
std::vector<ServerInfo> ServerLobby::serversVector;
std::atomic_bool ServerLobby::is_refreshing{ false };
std::atomic_bool ServerLobby::has_refreshed{ false };

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
	size_t realsize = size * nmemb;
	auto buff = static_cast<std::string*>(userp);
	size_t prev_size = buff->size();
	buff->resize(prev_size + realsize);
	memcpy((char*)buff->data() + prev_size, contents, realsize);
	return realsize;
}

void ServerLobby::FillOnlineRooms() {
	mainGame->roomListTable->clearRows();
	auto& rooms = roomsVector;

	std::wstring searchText(Utils::ToUpperNoAccents<std::wstring>(mainGame->ebRoomName->getText()));

	int searchRules = mainGame->cbFilterRule->getSelected();
	int searchBanlist = mainGame->cbFilterBanlist->getSelected();
	int bestOf = 0;
	int team1 = 0;
	int team2 = 0;
	try {
		bestOf = std::stoi(mainGame->ebOnlineBestOf->getText());
		team1 = std::stoi(mainGame->ebOnlineTeam1->getText());
		team2 = std::stoi(mainGame->ebOnlineTeam2->getText());
	}
	catch(...) {}

	bool doFilter = searchText.size() || searchRules > 0 || searchBanlist > 0 || bestOf || team1 || team2 || mainGame->btnFilterRelayMode->isPressed();

	const auto& normal_room = skin::ROOMLIST_NORMAL_ROOM_VAL;
	const auto& custom_room = skin::ROOMLIST_CUSTOM_ROOM_VAL;
	const auto& started_room = skin::ROOMLIST_STARTED_ROOM_VAL;
	const bool show_password_checked = mainGame->chkShowPassword->isChecked();
	const bool show_started_checked = mainGame->chkShowActiveRooms->isChecked();
	for(auto& room : rooms) {
		if((room.locked && !show_password_checked) || (room.started && !show_started_checked)) {
			continue;
		}
		if(doFilter) {
			if(searchText.size()) {
				auto res = Utils::ToUpperNoAccents(room.description).find(searchText);
				for(auto it = room.players.cbegin(); res == std::wstring::npos && it != room.players.cend(); it++)
					res = Utils::ToUpperNoAccents(*it).find(searchText);
				if(res == std::wstring::npos)
					continue;
			}
			if(bestOf && room.info.best_of != bestOf)
				continue;
			if(team1 && room.info.team1 != team1)
				continue;
			if(team2 && room.info.team2 != team2)
				continue;
			if(searchRules > 0) {
				if(searchRules != room.info.rule + 1)
					continue;
			}
			/*add mutex for banlist access*/
			if(searchBanlist > 0) {
				if(room.info.lflist != gdeckManager->_lfList[searchBanlist - 1].hash) {
					continue;
				}
			}
			if(mainGame->btnFilterRelayMode->isPressed() && !(room.info.duel_flag_low & DUEL_RELAY))
				continue;
		}

		epro::wstringview banlist = L"???";

		for(auto& list : gdeckManager->_lfList) {
			if(list.hash == room.info.lflist) {
				banlist = list.listName;
				break;
			}
		}

		irr::gui::IGUITable* roomListTable = mainGame->roomListTable;
		int index = roomListTable->getRowCount();

		roomListTable->addRow(index);
		roomListTable->setCellData(index, 0, room.locked ? (void*)1 : nullptr);
		roomListTable->setCellData(index, 1, &room);
		roomListTable->setCellText(index, 1, gDataManager->GetSysString(room.info.rule + 1900).data());
		roomListTable->setCellText(index, 2, epro::format(L"[{}vs{}]{}{}", room.info.team1, room.info.team2,
			(room.info.best_of > 1) ? epro::format(L" (best of {})", room.info.best_of) : L"",
			(room.info.duel_flag_low & DUEL_RELAY) ? L" (Relay)" : L"").data());
		int rule;
		auto duel_flag = (((uint64_t)room.info.duel_flag_low) | ((uint64_t)room.info.duel_flag_high) << 32);
		mainGame->GetMasterRule(duel_flag & ~(DUEL_RELAY | DUEL_TCG_SEGOC_NONPUBLIC | DUEL_PSEUDO_SHUFFLE), room.info.forbiddentypes, &rule);
		if(rule == 6) {
			if(duel_flag == DUEL_MODE_GOAT) {
				roomListTable->setCellText(index, 3, L"GOAT");
			} else if(duel_flag == DUEL_MODE_RUSH) {
				roomListTable->setCellText(index, 3, L"Rush");
			} else if(duel_flag == DUEL_MODE_SPEED) {
				roomListTable->setCellText(index, 3, L"Speed");
			} else
				roomListTable->setCellText(index, 3, L"Custom");
		} else
			roomListTable->setCellText(index, 3, epro::format(L"{}MR {}",
															 (duel_flag & DUEL_TCG_SEGOC_NONPUBLIC) ? L"TCG " : L"",
															 (rule == 0) ? 3 : rule).data());
		roomListTable->setCellText(index, 4, banlist.data());
		std::wstring players;
		for(const auto& player : room.players)
			players.append(player).append(L", ");
		if(players.size())
			players.erase(players.size() - 2);
		roomListTable->setCellText(index, 5, players.data());
		roomListTable->setCellText(index, 6, room.description.data());
		roomListTable->setCellText(index, 7, room.started ? gDataManager->GetSysString(1986).data() : gDataManager->GetSysString(1987).data());

		static constexpr DeckSizes normal_sizes{ {40,60}, {0,15}, {0,15} };

		irr::video::SColor color;
		if(room.started)
			color = started_room;
		else if(rule == 5 && !room.info.no_check_deck_content && room.info.sizes == normal_sizes && !room.info.no_shuffle_deck && room.info.start_lp == 8000 && room.info.start_hand == 5 && room.info.draw_count == 1)
			color = normal_room;
		else
			color = custom_room;
		roomListTable->setCellColor(index, 0, color);
		roomListTable->setCellColor(index, 1, color);
		roomListTable->setCellColor(index, 2, color);
		roomListTable->setCellColor(index, 3, color);
		roomListTable->setCellColor(index, 4, color);
		roomListTable->setCellColor(index, 5, color);
		roomListTable->setCellColor(index, 6, color);
		roomListTable->setCellColor(index, 7, color);
	}
	mainGame->roomListTable->setActiveColumn(mainGame->roomListTable->getActiveColumn(), true);
	if(!roomsVector.empty() && mainGame->chkShowActiveRooms->isChecked()) {
		mainGame->roomListTable->setActiveColumn(7, true);
		mainGame->roomListTable->orderRows(-1, irr::gui::EGOM_DESCENDING);
	}

	GUIUtils::ChangeCursor(mainGame->device, irr::gui::ECI_NORMAL);
	mainGame->btnLanRefresh2->setEnabled(true);
	mainGame->serverChoice->setEnabled(true);
	mainGame->roomListTable->setVisible(true);
	has_refreshed = false;
}
void ServerLobby::GetRoomsThread() {
	Utils::SetThreadName("RoomlistFetch");
	auto selected = mainGame->serverChoice->getSelected();
	if (selected < 0) return;
	const auto& serverInfo = serversVector[selected];

	mainGame->btnLanRefresh2->setEnabled(false);
	mainGame->serverChoice->setEnabled(false);
	mainGame->roomListTable->setVisible(false);

	std::string retrieved_data;
	char curl_error_buffer[CURL_ERROR_SIZE];
	CURL* curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, curl_error_buffer);
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl_handle, CURLOPT_URL, epro::format("{}://{}:{}/api/getrooms", serverInfo.GetProtocolString(), serverInfo.roomaddress, serverInfo.roomlistport).data());
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 60L);
	curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 15L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &retrieved_data);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, ygo::Utils::GetUserAgent().data());

	curl_easy_setopt(curl_handle, CURLOPT_NOPROXY, "*");
	curl_easy_setopt(curl_handle, CURLOPT_DNS_CACHE_TIMEOUT, 0);
	if(gGameConfig->ssl_certificate_path.size() && Utils::FileExists(Utils::ToPathString(gGameConfig->ssl_certificate_path)))
		curl_easy_setopt(curl_handle, CURLOPT_CAINFO, gGameConfig->ssl_certificate_path.data());

	const auto res = curl_easy_perform(curl_handle);
	curl_easy_cleanup(curl_handle);
	if(res != CURLE_OK) {
		if(gGameConfig->logDownloadErrors) {
			ErrorLog("Error updating the room list:");
			ErrorLog("Curl error: ({}) {} ({})", static_cast<std::underlying_type_t<CURLcode>>(res), curl_easy_strerror(res), curl_error_buffer);
		}
		//error
		mainGame->PopupMessage(gDataManager->GetSysString(2037));
		mainGame->btnLanRefresh2->setEnabled(true);
		mainGame->serverChoice->setEnabled(true);
		mainGame->roomListTable->setVisible(true);
		is_refreshing = false;
		has_refreshed = true;
		return;
	}

	roomsVector.clear();
	try {
		auto j = nlohmann::json::parse(retrieved_data);
		if(j.size()) {
#define GET(field, type) obj[field].get<type>()
			for(auto& obj : j["rooms"]) {
				RoomInfo room;
				room.id = GET("roomid", int);
				room.name = BufferIO::DecodeUTF8(obj["roomname"].get_ref<std::string&>());
				room.description = BufferIO::DecodeUTF8(obj["roomnotes"].get_ref<std::string&>());
				room.locked = GET("needpass", bool);
				room.started = obj["istart"].get_ref<std::string&>() == "start";
				room.info.mode = GET("roommode", int);
				room.info.team1 = GET("team1", int);
				room.info.team2 = GET("team2", int);
				room.info.best_of = GET("best_of", int);
				const auto flag = GET("duel_flag", uint64_t);
				room.info.duel_flag_low = flag & 0xffffffff;
				room.info.duel_flag_high = (flag >> 32) & 0xffffffff;
				room.info.forbiddentypes = GET("forbidden_types", int);
				room.info.extra_rules = GET("extra_rules", int);
				room.info.start_lp = GET("start_lp", int);
				room.info.start_hand = GET("start_hand", int);
				room.info.draw_count = GET("draw_count", int);
				room.info.time_limit = GET("time_limit", int);
				room.info.rule = GET("rule", int);
				room.info.no_check_deck_content = GET("no_check", bool);
				room.info.no_shuffle_deck = GET("no_shuffle", bool) || (flag & DUEL_PSEUDO_SHUFFLE);
				room.info.lflist = GET("banlist_hash", int);
				room.info.sizes.main.min = GET("main_min", uint16_t);
				room.info.sizes.main.max = GET("main_max", uint16_t);
				room.info.sizes.extra.min = GET("extra_min", uint16_t);
				room.info.sizes.extra.max = GET("extra_max", uint16_t);
				room.info.sizes.side.min = GET("side_min", uint16_t);
				room.info.sizes.side.max = GET("side_max", uint16_t);
#undef GET
				for(auto& obj2 : obj["users"])
					room.players.push_back(BufferIO::DecodeUTF8(obj2["name"].get_ref<const std::string&>()));

				roomsVector.push_back(std::move(room));
			}
		}
	} catch(const std::exception& e) {
		ErrorLog("Exception occurred parsing server rooms: {}", e.what());
	}
	has_refreshed = true;
	is_refreshing = false;
}
bool ServerLobby::IsKnownHost(epro::Host host) {
	return std::find_if(serversVector.begin(), serversVector.end(), [&](const ServerInfo& konwn_host) {
		return konwn_host.Resolved() == host;
	}) != serversVector.end();
}
void ServerLobby::RefreshRooms() {
	if(is_refreshing)
		return;
	is_refreshing = true;
	mainGame->roomListTable->clearRows();
	GUIUtils::ChangeCursor(mainGame->device, irr::gui::ECI_WAIT);
	epro::thread(GetRoomsThread).detach();
}
bool ServerLobby::HasRefreshedRooms() {
	return has_refreshed;
}
void ServerLobby::JoinServer(bool host) {
	mainGame->ebNickName->setText(mainGame->ebNickNameOnline->getText());
	auto selected = mainGame->serverChoice->getSelected();
	if (selected < 0) return;
	const auto& serverinfo = serversVector[selected].Resolved();
	if(serverinfo.address.family == epro::Address::UNK)
		return;
	if(host) {
		if(!DuelClient::StartClient(serverinfo.address, serverinfo.port))
			return;
	} else {
		//client
		auto room = static_cast<RoomInfo*>(mainGame->roomListTable->getCellData(mainGame->roomListTable->getSelected(), 1));
		if(!room)
			return;
		if(room->locked) {
			if(!mainGame->wRoomPassword->isVisible()) {
				mainGame->wRoomPassword->setVisible(true);
				return;
			}
			auto text = mainGame->ebRPName->getText();
			if(*text == L'\0')
				return;
			mainGame->wRoomPassword->setVisible(false);
			mainGame->dInfo.secret.pass = text;
		} else
			mainGame->dInfo.secret.pass.clear();
		if(!DuelClient::StartClient(serverinfo.address, serverinfo.port, room->id, false))
			return;
	}
}

const epro::Host& ServerInfo::Resolved() const {
	if(!resolved) {
		try {
			resolved_address = epro::Host::resolve(address, duelport);
			resolved = true;
		} catch(const std::exception& e) {
			ErrorLog("Exception occurred: {}", e.what());
		}
	}
	return resolved_address;
}

}
