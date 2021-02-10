#include "windbot.h"
#include "utils.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(__ANDROID__)
#include "Android/porting_android.h"
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <fmt/format.h>
#include "config.h"
#include "bufferio.h"
#include "logging.h"

namespace ygo {

epro::path_string WindBot::executablePath{};
uint32_t WindBot::version{ CLIENT_VERSION };

#if defined(_WIN32) || defined(__ANDROID__)
bool WindBot::Launch(int port, const std::wstring& pass, bool chat, int hand) const {
#ifdef _WIN32
	auto args = Utils::ToPathString(fmt::format(
		///kdiy//////////
		//L"./WindBot/WindBot.exe HostInfo=\"{}\" Deck=\"{}\" Port={} Version={} name=\"[AI] {}\" Chat={} {}",
		L"./WindBot/WindBot.exe HostInfo=\"{}\" Deck=\"{}\" Port={} Version={} name=\"[AI] {}\" Dialog=\"{}\" Deckfolder=\"{}\" Deckpath=\"{}\" Chat={} {}",		
		///kdiy//////////		
		pass,
		deck,
		port,
		version,
		name,
		///kdiy//////////	
		dialog,
		deckfolder,
		deckpath,
		///kdiy//////////	
		chat,
		hand ? fmt::format(L" Hand={}", hand) : L""));
	STARTUPINFO si{};
	PROCESS_INFORMATION pi{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	if(CreateProcess(nullptr, (TCHAR*)args.data(), nullptr, nullptr, FALSE, 0, nullptr, executablePath.data(), &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return true;
	}
	return false;
#elif defined(__ANDROID__)
	std::string param = fmt::format(
		////////kdiy//////
		//"HostInfo='{}' Deck='{}' Port={} Version={} Name='[AI] {}' Chat={} Hand={}",
		"HostInfo='{}' Deck='{}' Port={} Version={} Name='[AI] {}' Dialog='{}' Deckfolder='{}'  Deckpath='{}' Chat={} Hand={}",		
		////////kdiy//////
		BufferIO::EncodeUTF8s(pass),
		BufferIO::EncodeUTF8s(deck),
		port,
		version,
		BufferIO::EncodeUTF8s(name),
		/////kdiy//////
		BufferIO::EncodeUTF8s(dialog),
		BufferIO::EncodeUTF8s(deckfolder),
		BufferIO::EncodeUTF8s(deckpath),
		/////kdiy//////
		static_cast<int>(chat),
		hand);
	porting::launchWindbot(param);
	return true;
#endif
}
#else
pid_t WindBot::Launch(int port, const std::wstring& pass, bool chat, int hand) const {
	std::string argPass = fmt::format("HostInfo={}", BufferIO::EncodeUTF8s(pass));
	std::string argDeck = fmt::format("Deck={}", BufferIO::EncodeUTF8s(deck));
	std::string argPort = fmt::format("Port={}", port);
	std::string argVersion = fmt::format("Version={}", version);
	std::string argName = fmt::format("name=[AI] {}", BufferIO::EncodeUTF8s(name));
	///////////kdiy//////////
	std::string argDialog = fmt::format("Dialog={}", BufferIO::EncodeUTF8s(dialog));	
	std::string argDeckfolder = fmt::format("Deckfolder={}", BufferIO::EncodeUTF8s(deckfolder));		
	std::string argDeckpath = fmt::format("Deckpath={}", BufferIO::EncodeUTF8s(deckpath));		
	///////////kdiy//////////
	std::string argChat = fmt::format("Chat={}", chat);
	std::string argHand = fmt::format("Hand={}", hand);
	std::string oldpath;
	if(executablePath.size()) {
		oldpath = getenv("PATH");
		std::string envPath = fmt::format("{}:{}", oldpath, executablePath);
		setenv("PATH", envPath.data(), true);
	}
	auto pid = vfork();
	if(pid == 0) {
		execlp("mono", "WindBot.exe", "./WindBot/WindBot.exe",
		       ///////kdiy//////////	
			   //argPass.data(), argDeck.data(), argPort.data(), argVersion.data(), argName.data(), argChat.data(),
			   argPass.data(), argDeck.data(), argPort.data(), argVersion.data(), argName.data(), argDialog.data(), argDeckfolder.data(), argDeckpath.data(), argChat.data(),
			   ///////kdiy//////////	
			   "AssetPath=./WindBot", hand ? argHand.data() : nullptr, nullptr);
		_exit(EXIT_FAILURE);
	}
	if(waitpid(pid, nullptr, WNOHANG) != 0)
		pid = 0;
	if(executablePath.size())
		setenv("PATH", oldpath.data(), true);
	return pid;
}
#endif

}
