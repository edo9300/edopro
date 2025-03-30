#include "config.h"
#include <event2/thread.h>
#include <IrrlichtDevice.h>
#include <IGUIButton.h>
#include <IGUICheckBox.h>
#include <IGUIEditBox.h>
#include <IGUIWindow.h>
#include <IGUIEnvironment.h>
#include <ISceneManager.h>
#include "client_updater.h"
#include "cli_args.h"
#include "config.h"
#include "data_handler.h"
#include "logging.h"
#include "game.h"
#include "log.h"
#include "joystick_wrapper.h"
#include "utils_gui.h"
#include "fmt.h"
#include "curl.h"
#if EDOPRO_MACOS
#include "osx_menu.h"
#endif

bool is_from_discord = false;
bool open_file = false;
epro::path_string open_file_name = EPRO_TEXT("");
bool show_changelog = false;
ygo::Game* ygo::mainGame = nullptr;
ygo::ImageDownloader* ygo::gImageDownloader = nullptr;
ygo::DataManager* ygo::gDataManager = nullptr;
ygo::SoundManager* ygo::gSoundManager = nullptr;
ygo::GameConfig* ygo::gGameConfig = nullptr;
ygo::RepoManager* ygo::gRepoManager = nullptr;
ygo::DeckManager* ygo::gdeckManager = nullptr;
ygo::ClientUpdater* ygo::gClientUpdater = nullptr;
JWrapper* gJWrapper = nullptr;

namespace {
void CheckArguments(const args_t& args) {
	if(args[LAUNCH_PARAM::MUTE].enabled) {
		ygo::GUIUtils::SetCheckbox(ygo::mainGame->device, ygo::mainGame->tabSettings.chkEnableSound, false);
		ygo::GUIUtils::SetCheckbox(ygo::mainGame->device, ygo::mainGame->tabSettings.chkEnableMusic, false);
	}
	if(args[LAUNCH_PARAM::SET_NICKNAME].enabled && !args[LAUNCH_PARAM::SET_NICKNAME].argument.empty()) {
		auto nickname = ygo::Utils::ToUnicodeIfNeeded(args[LAUNCH_PARAM::SET_NICKNAME].argument);
		ygo::mainGame->ebNickName->setText(nickname.c_str());
		ygo::mainGame->ebNickNameOnline->setText(nickname.c_str());
	}
	if(args[LAUNCH_PARAM::SET_DECK].enabled && !args[LAUNCH_PARAM::SET_DECK].argument.empty()) {
		auto selectedDeck = ygo::Utils::ToUTF8IfNeeded(args[LAUNCH_PARAM::SET_DECK].argument);
		ygo::mainGame->TrySetDeck(selectedDeck);
	}
	if(args[LAUNCH_PARAM::EXIT_AFTER].enabled) {
		ygo::mainGame->exitAfter = true;
	}
	if(args[LAUNCH_PARAM::REPLAY].enabled && !args[LAUNCH_PARAM::REPLAY].argument.empty()) {
		auto replay = ygo::Utils::ToPathString(args[LAUNCH_PARAM::REPLAY].argument);
		ygo::mainGame->LaunchReplay(replay);
	}
	if(args[LAUNCH_PARAM::HOST].enabled && !args[LAUNCH_PARAM::HOST].argument.empty()) {
		auto host_params = ygo::Utils::ToUTF8IfNeeded(args[LAUNCH_PARAM::HOST].argument);
		ygo::mainGame->LaunchHost(host_params);
	}
	if(args[LAUNCH_PARAM::JOIN].enabled && !args[LAUNCH_PARAM::JOIN].argument.empty()) {
		auto join_params = ygo::Utils::ToUTF8IfNeeded(args[LAUNCH_PARAM::JOIN].argument);
		ygo::mainGame->LaunchJoin(join_params);
	}
	if(args[LAUNCH_PARAM::DECKBUILDER].enabled && !args[LAUNCH_PARAM::DECKBUILDER].argument.empty()) {
		auto deckbuilder_params = ygo::Utils::ToUTF8IfNeeded(args[LAUNCH_PARAM::DECKBUILDER].argument);
		ygo::mainGame->LaunchDeckbuilder(deckbuilder_params);
	}
}

inline void ThreadsStartup() {
#if EDOPRO_WINDOWS
	const WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;
	auto wsaret = WSAStartup(wVersionRequested, &wsaData);
	if(wsaret != 0)
		throw std::runtime_error(epro::format("Failed to initialize WinSock ({})!", wsaret));
	if(evthread_use_windows_threads() < 0)
		throw std::runtime_error("Failed initialize libevent!");
#else
	if(evthread_use_pthreads() < 0)
		throw std::runtime_error("Failed initialize libevent!");
#endif
	auto res = curl_global_init(CURL_GLOBAL_SSL);
	if(res != CURLE_OK)
		throw std::runtime_error(epro::format("Curl error: ({}) {}", res, curl_easy_strerror(res)));
}
inline void ThreadsCleanup() {
	curl_global_cleanup();
	libevent_global_shutdown();
#if EDOPRO_WINDOWS
	WSACleanup();
#endif
}

//clang below version 11 (llvm version 8) has a bug with brace class initialization
//where it can't properly deduce the destructors of its members
//https://reviews.llvm.org/D45898
//https://bugs.llvm.org/show_bug.cgi?id=28280
//add a workaround to construct the game object indirectly
//to avoid constructing it with brace initialization
#if defined(__clang_major__) && __clang_major__ <= 10
class Game {
	ygo::Game game;
public:
	Game() :game() {};
	ygo::Game* operator&() { return &game; }
};
#else
using Game = ygo::Game;
#endif

}

#if EDOPRO_WINDOWS
#define ADMIN_STR "administrator"
#else
#define ADMIN_STR "root"
#endif

static bool args_require_repo_read_only() {
	return cli_args[LAUNCH_PARAM::REPLAY].enabled
		|| cli_args[LAUNCH_PARAM::HOST].enabled
		|| cli_args[LAUNCH_PARAM::JOIN].enabled
		|| cli_args[LAUNCH_PARAM::DECKBUILDER].enabled;
}

int edopro_main(const args_t& args) {
	std::puts(EDOPRO_VERSION_STRING_DEBUG);
	if(ygo::Utils::IsRunningAsAdmin() && !args[LAUNCH_PARAM::WANTS_TO_RUN_AS_ADMIN].enabled) {
		constexpr auto err = "Attempted to run the game as " ADMIN_STR ".\n"
			"You should NEVER have to run the game with elevated priviledges.\n"
			"If for some reason you REALLY want to do that, launch the game with the option \"-i-want-to-be-admin\""sv;
		epro::print("{}\n", err);
		ygo::GUIUtils::ShowErrorWindow("Initialization fail", err);
		return EXIT_FAILURE;
	}
	{
		const auto& workdir = args[LAUNCH_PARAM::WORK_DIR];
		const epro::path_stringview dest = workdir.enabled ? workdir.argument : ygo::Utils::GetExeFolder();
		if(!ygo::Utils::SetWorkingDirectory(dest)) {
			const auto err = epro::format("failed to change directory to: {} ({})",
										 ygo::Utils::ToUTF8IfNeeded(dest), ygo::Utils::GetLastErrorString());
			ygo::ErrorLog(err);
			epro::print("{}\n", err);
			ygo::GUIUtils::ShowErrorWindow("Initialization fail", err);
			return EXIT_FAILURE;
		}
	}
	ygo::Utils::SetupCrashDumpLogging();
	try {
		ThreadsStartup();
		std::atexit(ThreadsCleanup);
	} catch(const std::exception& e) {
		epro::stringview text(e.what());
		ygo::ErrorLog(text);
		epro::print("{}\n", text);
		ygo::GUIUtils::ShowErrorWindow("Initialization fail", text);
		return EXIT_FAILURE;
	}
	if(args_require_repo_read_only()) {
		cli_args[LAUNCH_PARAM::REPOS_READ_ONLY].enabled = true;
	}
	show_changelog = args[LAUNCH_PARAM::CHANGELOG].enabled;
	ygo::ClientUpdater updater(args[LAUNCH_PARAM::OVERRIDE_UPDATE_URL].argument);
	ygo::gClientUpdater = &updater;
	std::unique_ptr<ygo::DataHandler> data{ nullptr };
	try {
		data = std::make_unique<ygo::DataHandler>();
		ygo::gImageDownloader = data->imageDownloader.get();
		ygo::gDataManager = data->dataManager.get();
		ygo::gSoundManager = data->sounds.get();
		ygo::gGameConfig = data->configs.get();
		ygo::gRepoManager = data->gitManager.get();
		ygo::gdeckManager = data->deckManager.get();
	}
	catch(const std::exception& e) {
		epro::stringview text(e.what());
		ygo::ErrorLog(text);
		epro::print("{}\n", text);
		ygo::GUIUtils::ShowErrorWindow("Initialization fail", text);
		return EXIT_FAILURE;
	}
	if (!data->configs->noClientUpdates)
		updater.CheckUpdates();
#if EDOPRO_WINDOWS
	if(!data->configs->showConsole) {
		fclose(stdin);
		fclose(stderr);
		fclose(stdout);
		FreeConsole();
	}
#endif
#if EDOPRO_MACOS
	EDOPRO_SetupMenuBar([]() {
		ygo::gGameConfig->fullscreen = !ygo::gGameConfig->fullscreen;
		ygo::mainGame->gSettings.chkFullscreen->setChecked(ygo::gGameConfig->fullscreen);
	});
#endif
	srand(static_cast<uint32_t>(time(nullptr)));
	std::unique_ptr<JWrapper> joystick{ nullptr };
	bool firstlaunch = true;
	bool reset = false;
	do {
		Game _game{};
		ygo::mainGame = &_game;
		std::swap(data->tmp_device, ygo::mainGame->device);
		try {
			ygo::mainGame->Initialize();
		}
		catch(const std::exception& e) {
			epro::stringview text(e.what());
			ygo::ErrorLog(text);
			epro::print("{}\n", text);
			ygo::GUIUtils::ShowErrorWindow("Assets load fail", text);
			return EXIT_FAILURE;
		}
		if(firstlaunch) {
			joystick = std::make_unique<JWrapper>(ygo::mainGame->device.get());
			gJWrapper = joystick.get();
			firstlaunch = false;
			CheckArguments(args);
		}
		reset = ygo::mainGame->MainLoop();
		std::swap(data->tmp_device, ygo::mainGame->device);
		if(reset) {
			auto device = data->tmp_device;
			device->setEventReceiver(nullptr);
			auto driver = device->getVideoDriver();
			/*the gles drivers have an additional cache, that isn't cleared when the textures are removed,
			since it's not a big deal clearing them, as they'll be reused, they aren't cleared*/
			/*driver->removeAllTextures();*/
			driver->removeAllHardwareBuffers();
			driver->removeAllOcclusionQueries();
			device->getSceneManager()->clear();
			auto env = device->getGUIEnvironment();
			env->clear();
		}
	} while(reset);
	return EXIT_SUCCESS;
}
