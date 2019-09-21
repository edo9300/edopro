#include "config.h"
#include "game.h"
#include "data_manager.h"
#include <event2/thread.h>
#include <memory>
#ifdef __APPLE__
#import <CoreFoundation/CoreFoundation.h>
#endif

int main(int argc, char* argv[]) {
#ifndef _WIN32
	setlocale(LC_CTYPE, "UTF-8");
#endif
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);
	evthread_use_windows_threads();
#else
	evthread_use_pthreads();
#endif //_WIN32
	std::string corepath;
	if(argc > 1)
		corepath = argv[1];
	ygo::Game _game;
	ygo::mainGame = &_game;
	int ret = ygo::mainGame->MainServerLoop(corepath);
#ifdef _WIN32
	WSACleanup();
#endif //_WIN32
	return ret;
}
