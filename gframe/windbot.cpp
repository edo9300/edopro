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

#if !defined(_WIN32) && !defined(__ANDROID__)
epro::path_string WindBot::executablePath{};
#endif
uint32_t WindBot::version{ CLIENT_VERSION };

WindBot::launch_ret_t WindBot::Launch(int port, epro::wstringview pass, bool chat, int hand) const {
#ifdef _WIN32
	//Windows can modify this string
	auto args = Utils::ToPathString(fmt::format(
		///kdiy//////////
		//L"WindBot.exe HostInfo=\"{}\" Deck=\"{}\" Port={} Version={} name=\"[AI] {}\" Chat={} Hand={} AssetPath=./WindBot",
		L"WindBot.exe HostInfo=\"{}\" Deck=\"{}\" Port={} Version={} name=\"[AI] {}\" Dialog=\"{}\" Deckfolder=\"{}\" Deckpath=\"{}\" Chat={} Hand={} AssetPath=./WindBot",		
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
		hand));
	STARTUPINFO si{ sizeof(si) };
	PROCESS_INFORMATION pi{};
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	if(CreateProcess(EPRO_TEXT("./WindBot/WindBot.exe"), &args[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return true;
	}
	return false;
#elif defined(__ANDROID__)
	auto param = BufferIO::EncodeUTF8(fmt::format(
		////////kdiy//////
		//L"HostInfo='{}' Deck='{}' Port={} Version={} Name='[AI] {}' Chat={} Hand={}",
		L"HostInfo='{}' Deck='{}' Port={} Version={} Name='[AI] {}' Dialog='{}' Deckfolder='{}' Deckpath='{}' Chat={} Hand={}",		
		////////kdiy//////
		pass,
		deck,
		port,
		version,
		name,
		/////kdiy//////
		dialog,
		deckfolder,
		deckpath,
		/////kdiy//////
		static_cast<int>(chat),
		hand));
	porting::launchWindbot(param);
	return true;
#else
	std::string argPass = fmt::format("HostInfo={}", BufferIO::EncodeUTF8(pass));
	std::string argDeck = fmt::format("Deck={}", BufferIO::EncodeUTF8(deck));
	std::string argPort = fmt::format("Port={}", port);
	std::string argVersion = fmt::format("Version={}", version);
	std::string argName = fmt::format("name=[AI] {}", BufferIO::EncodeUTF8(name));
	///////////kdiy//////////
	std::string argDialog = fmt::format("Dialog={}", BufferIO::EncodeUTF8(dialog));	
	std::string argDeckfolder = fmt::format("Deckfolder={}", BufferIO::EncodeUTF8(deckfolder));		
	std::string argDeckpath = fmt::format("Deckpath={}", BufferIO::EncodeUTF8(deckpath));
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
	if(executablePath.size())
		setenv("PATH", oldpath.data(), true);
	if(pid < 0 || waitpid(pid, nullptr, WNOHANG) != 0)
		pid = 0;
	return pid;
#endif
}

}
