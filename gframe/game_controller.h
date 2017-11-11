#pragma once
#include <SDL.h>
#include "game.h"

namespace ygo
{
	enum rooms
	{
		GC_ROOM_MAINMENU,
		GC_ROOM_LAN_CONNECT,
		GC_ROOM_LAN_SELECT_DECK,
		GC_ROOM_PUZZLE,
		GC_ROOM_REPLAY,
		GC_ROOM_BUILDING,
		GC_ROOM_DUELING,
		GC_ROOM_SIDING,
	};
	enum buttons
	{
		GC_BUTTON_FORWARD,
		GC_BUTTON_BACKWARD,

		GC_BUTTON_LEFT,
		GC_BUTTON_RIGHT,
		GC_BUTTON_UP,
		GC_BUTTON_DOWN,
	};
	class GameController
	{
		ygo::Game * game;
		SDL_Event e;
		SDL_GameController * gc;
	
		int itemIndex;
		float axis[2];
		ygo::rooms croom;

		void update_room();
		void handle_button(ygo::buttons, int);
	public:
		void init(ygo::Game *);
		void init();
		void end();

		void process_events();
	};
	extern GameController gameController;
}
