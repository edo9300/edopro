#include "windbot.h"
#include "utils.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <unistd.h>
#endif
#ifdef __ANDROID__
#include "Android/porting_android.h"
#endif
#include <fmt/format.h>
#include "bufferio.h"
#include "logging.h"

namespace ygo {

#if defined(_WIN32) || defined(__ANDROID__)
bool WindBot::Launch(int port, const std::wstring& pass, bool chat, int hand) const {
#else
pid_t WindBot::Launch(int port, const std::wstring& pass, bool chat, int hand) const {
#endif
#ifdef _WIN32
	auto args = fmt::format(
		///kdiy//////////
		//L"./WindBot/WindBot.exe HostInfo=\"{}\" Deck=\"{}\" Port={} Version={} name=\"[AI] {}\" Chat={} {}",
		L"./WindBot/WindBot.exe HostInfo=\"{}\" Deck=\"{}\" Port={} Version={} name=\"[AI] {}\" Dialog=\"{}\" Deckpath=\"{}\" Chat={} {}",		
		///kdiy//////////		
		pass,
		deck,
		port,
		version,
		name,
		///kdiy//////////	
		dialog,
		deckpath,
		///kdiy//////////	
		chat,
		hand ? fmt::format(L"Hand={}", hand) : L"");
	STARTUPINFO si = {};
	PROCESS_INFORMATION pi = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	if (CreateProcess(NULL, (TCHAR*)Utils::ToPathString(args).data(), NULL, NULL, FALSE, 0, NULL, executablePath.data(), &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return true;
	}
	return false;
#elif defined(__ANDROID__)
	std::string param = fmt::format(
		////////kdiy//////
		//"HostInfo='{}' Deck='{}' Port={} Version={} Name='[AI] {}' Chat={} Hand={}",
		"HostInfo='{}' Deck='{}' Port={} Version={} Name='[AI] {}' Dialog='{}' Deckpath='{}' Chat={} Hand={}",		
		////////kdiy//////
		BufferIO::EncodeUTF8s(pass),
		BufferIO::EncodeUTF8s(deck),
		port,
		version,
		BufferIO::EncodeUTF8s(name),
		/////kdiy//////
		BufferIO::EncodeUTF8s(dialog),
		BufferIO::EncodeUTF8s(deckpath),
		/////kdiy//////
		static_cast<int>(chat),
		hand);
	porting::launchWindbot(param);
	return true;
#else
	pid_t pid = fork();
	if (pid == 0) {
		std::string argPass = fmt::format("HostInfo={}", BufferIO::EncodeUTF8s(pass));
		std::string argDeck = fmt::format("Deck={}", BufferIO::EncodeUTF8s(deck));
		std::string argPort = fmt::format("Port={}", port);
		std::string argVersion = fmt::format("Version={}", version);
		std::string argName = fmt::format("name=[AI] {}", BufferIO::EncodeUTF8s(name));
		///////////kdiy//////////
		std::string argDialog = fmt::format("Dialog={}", BufferIO::EncodeUTF8s(dialog));	
		std::string argDeckpath = fmt::format("Deckpath={}", BufferIO::EncodeUTF8s(deckpath));		
		///////////kdiy//////////
		std::string argChat = fmt::format("Chat={}", chat);
		std::string argHand = fmt::format("Hand={}", hand);
		if(chdir("WindBot") == 0) {
			if(executablePath.length()) {
				std::string envPath = getenv("PATH") + (":" + executablePath);
				setenv("PATH", envPath.data(), true);
			}
			execlp("mono", "WindBot.exe", "WindBot.exe",
			       ///////kdiy//////////
				   //argPass.data(), argDeck.data(), argPort.data(), argVersion.data(), argName.data(), argChat.data(),
				   argPass.data(), argDeck.data(), argPort.data(), argVersion.data(), argName.data, argDialog.data(),  argDeckpath.data(), argChat.data(),
				   ///////kdiy//////////			   
				   hand ? argHand.data() : nullptr, nullptr);
		}
		auto message = fmt::format("Failed to start WindBot Ignite: {}", strerror(errno));
		if(chdir("..") != 0)
			ErrorLog(strerror(errno));
		ErrorLog(message);
		exit(EXIT_FAILURE);
	}
	return pid;
#endif
}

}
