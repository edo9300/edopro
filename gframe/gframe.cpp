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
#ifdef __APPLE__
	CFURLRef bundle_url = CFBundleCopyBundleURL(CFBundleGetMainBundle());
	CFURLRef bundle_base_url = CFURLCreateCopyDeletingLastPathComponent(NULL, bundle_url);
	CFRelease(bundle_url);
	CFStringRef path = CFURLCopyFileSystemPath(bundle_base_url, kCFURLPOSIXPathStyle);
	CFRelease(bundle_base_url);
	chdir(CFStringGetCStringPtr(path, kCFStringEncodingUTF8));
	CFRelease(path);
#endif //__APPLE__
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);
	evthread_use_windows_threads();
#else
	evthread_use_pthreads();
#endif //_WIN32
	ygo::Game _game;
	ygo::mainGame = &_game;
	ygo::aServerPort = 7911;
	ygo::game_info.lflist = 999;
	ygo::game_info.rule = 0;
	ygo::game_info.mode = 0;
	ygo::game_info.start_hand = 5;
	ygo::game_info.start_lp = 8000;
	ygo::game_info.draw_count = 1;
	ygo::game_info.no_check_deck = true;
	ygo::game_info.no_shuffle_deck = false;
	ygo::game_info.duel_rule = DEFAULT_DUEL_RULE;
	ygo::game_info.time_limit = 180;
	ygo::game_info.handshake = SERVER_HANDSHAKE;
	ygo::game_info.duel_flag = MASTER_RULE_4;
	ygo::game_info.forbiddentypes = MASTER_RULE_4_FORB;
	ygo::game_info.extra_rules = 0;
	if(argc > 1) {
		ygo::aServerPort = std::stoi(argv[1]);
		int lflist = std::stoi(argv[2]);
		if(lflist < 0)
			lflist = 999;
		ygo::game_info.lflist = lflist;
		ygo::game_info.rule = std::stoi(argv[3]);
		int mode = std::stoi(argv[4]);
		if(mode > 2)
			mode = 0;
		ygo::game_info.mode = mode;
		if(argv[5][0] == 'T') {
			ygo::game_info.duel_rule = DEFAULT_DUEL_RULE - 1;
			ygo::game_info.duel_flag = MASTER_RULE_3;
			ygo::game_info.forbiddentypes = MASTER_RULE_3_FORB;
		}
		else
			ygo::game_info.duel_rule = DEFAULT_DUEL_RULE;
		if(argv[6][0] == 'T')
			ygo::game_info.no_check_deck = true;
		else
			ygo::game_info.no_check_deck = false;
		if(argv[7][0] == 'T')
			ygo::game_info.no_shuffle_deck = true;
		else
			ygo::game_info.no_shuffle_deck = false;
		ygo::game_info.start_lp = std::stoi(argv[8]);
		ygo::game_info.start_hand = std::stoi(argv[9]);
		ygo::game_info.draw_count = std::stoi(argv[10]);
		ygo::game_info.time_limit = std::stoi(argv[11]);
	}
	if(ygo::game_info.mode == MODE_SINGLE) {
		ygo::game_info.team1 = 1;
		ygo::game_info.team2 = 1;
		ygo::game_info.best_of = 0;
	}
	if(ygo::game_info.mode == MODE_MATCH) {
		ygo::game_info.team1 = 1;
		ygo::game_info.team2 = 1;
		ygo::game_info.best_of = 3;
	}
	if(ygo::game_info.mode == MODE_TAG) {
		ygo::game_info.team1 = 2;
		ygo::game_info.team2 = 2;
		ygo::game_info.best_of = 0;
	}
	if(ygo::game_info.mode == MODE_RELAY) {
		ygo::game_info.team1 = 3;
		ygo::game_info.team2 = 3;
		ygo::game_info.best_of = 0;
		ygo::game_info.duel_flag |= DUEL_RELAY_MODE;
	}
	ygo::mainGame->MainServerLoop();
#ifdef _WIN32
	WSACleanup();
#endif //_WIN32
	return EXIT_SUCCESS;
}
