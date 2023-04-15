#include "windbot.h"
#include "utils.h"
#include "config.h"
#if EDOPRO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif EDOPRO_ANDROID
#include "porting.h"
#include <nlohmann/json.hpp>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <fmt/format.h>
#if !EDOPRO_ANDROID
#include "Base64.h"
#endif
#include "config.h"
#include "bufferio.h"
#include "logging.h"

namespace ygo {

#if EDOPRO_LINUX || EDOPRO_MACOS
epro::path_string WindBot::executablePath{};
#endif
static constexpr uint32_t version{ CLIENT_VERSION };
#if !EDOPRO_ANDROID
nlohmann::ordered_json WindBot::databases{};
bool WindBot::serialized{ false };
decltype(WindBot::serialized_databases) WindBot::serialized_databases{};
#endif

WindBot::launch_ret_t WindBot::Launch(int port, epro::wstringview pass, bool chat, int hand, const wchar_t* overridedeck) const {
#if !EDOPRO_ANDROID
	if(!serialized) {
		serialized = true;
		serialized_databases = base64_encode<decltype(serialized_databases)>(databases.dump(-1, ' ', false, nlohmann::detail::error_handler_t::replace));
	}
#endif
#if EDOPRO_WINDOWS
	//Windows can modify this string
	auto args = Utils::ToPathString(epro::format(
		L"WindBot.exe HostInfo=\"{}\" Deck=\"{}\" Port={} Version={} name=\"[AI] {}\" Chat={} Hand={} DbPaths={}{} AssetPath=./WindBot",
		pass, deck, port, version, name, chat, hand, serialized_databases, overridedeck ? epro::format(L" DeckFile=\"{}\"", overridedeck) : L""));
	STARTUPINFO si{ sizeof(si) };
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi;
	if(!CreateProcess(EPRO_TEXT("./WindBot/WindBot.exe"), &args[0], nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi))
		return false;
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
#elif EDOPRO_ANDROID
	nlohmann::json param({
							{"HostInfo", BufferIO::EncodeUTF8(pass)},
							{"Deck", BufferIO::EncodeUTF8(deck)},
							{"Port", fmt::to_string(port)},
							{"Version", fmt::to_string(version)},
							{"Name", BufferIO::EncodeUTF8(name)},
							{"Chat", fmt::to_string(static_cast<int>(chat))},
							{"Hand", fmt::to_string(hand)}
						  });
	if(overridedeck)
		param["DeckFile"] = BufferIO::EncodeUTF8(overridedeck);
	porting::launchWindbot(param.dump());
	return true;
#elif EDOPRO_LINUX || EDOPRO_MACOS
	std::string argPass = epro::format("HostInfo={}", BufferIO::EncodeUTF8(pass));
	std::string argDeck = epro::format("Deck={}", BufferIO::EncodeUTF8(deck));
	std::string argPort = epro::format("Port={}", port);
	std::string argVersion = epro::format("Version={}", version);
	std::string argName = epro::format("name=[AI] {}", BufferIO::EncodeUTF8(name));
	std::string argChat = epro::format("Chat={}", chat);
	std::string argHand = epro::format("Hand={}", hand);
	std::string argDbPaths = epro::format("DbPaths={}", serialized_databases);
	std::string argDeckFile;
	if(overridedeck)
		argDeckFile = epro::format("DeckFile={}", BufferIO::EncodeUTF8(overridedeck));
	std::string oldpath;
	if(executablePath.size()) {
		oldpath = getenv("PATH");
		std::string envPath = epro::format("{}:{}", oldpath, executablePath);
		setenv("PATH", envPath.data(), true);
	}
	pid_t pid;
	{
		const char* argPass_cstr = argPass.data();
		const char* argDeck_cstr = argDeck.data();
		const char* argPort_cstr = argPort.data();
		const char* argVersion_cstr = argVersion.data();
		const char* argName_cstr = argName.data();
		const char* argChat_cstr = argChat.data();
		const char* argHand_cstr = argHand.data();
		const char* argDbPaths_cstr = argDbPaths.data();
		const char* argDeckFile_cstr = overridedeck ? argDeckFile.data() : nullptr;
		pid = vfork();
		if(pid == 0) {
			execlp("mono", "WindBot.exe", "./WindBot/WindBot.exe",
				   argPass_cstr, argDeck_cstr, argPort_cstr, argVersion_cstr, argName_cstr, argChat_cstr,
				   argDbPaths_cstr, "AssetPath=./WindBot", argHand_cstr, argDeckFile_cstr, nullptr);
			_exit(EXIT_FAILURE);
		}
	}
	if(executablePath.size())
		setenv("PATH", oldpath.data(), true);
	if(pid < 0 || waitpid(pid, nullptr, WNOHANG) != 0)
		pid = 0;
	return pid;
#endif
}

std::wstring WindBot::GetLaunchParameters(int port, epro::wstringview pass, bool chat, int hand, const wchar_t* overridedeck) const {
#if !EDOPRO_ANDROID
	if(!serialized) {
		serialized = true;
		serialized_databases = base64_encode<decltype(serialized_databases)>(databases.dump(-1, ' ', false, nlohmann::detail::error_handler_t::replace));
	}
	const auto assets_path = Utils::GetAbsolutePath(EPRO_TEXT("./WindBot"_sv));
	const auto override_deck = overridedeck ? epro::format(L" DeckFile=\"{}\"", overridedeck) : L"";
	return epro::format(
		L"HostInfo=\"{}\" Deck=\"{}\" Port={} Version={} name=\"[AI] {}\" Chat={} Hand={} DbPaths={}{} AssetPath=\"{}\"",
		pass, deck, port, version, name, chat, hand, Utils::ToUnicodeIfNeeded(serialized_databases), override_deck, Utils::ToUnicodeIfNeeded(assets_path));
#else
	return {};
#endif
}

void WindBot::AddDatabase(epro::path_stringview database) {
#if EDOPRO_ANDROID
	porting::addWindbotDatabase(Utils::GetAbsolutePath(database));
#else
	serialized = false;
	databases.push_back(Utils::ToUTF8IfNeeded(Utils::GetAbsolutePath(database)));
#endif
}

}
