#include "game_controller.h"
#include <iostream>
#include <algorithm> // std::clamp

extern void ClickButton(irr::gui::IGUIElement* btn); // from gframe.cpp

namespace ygo
{
	GameController gameController;
	
	void GameController::init(ygo::Game * game_attached_to)
	{
		game = game_attached_to;

		croom = rooms::GC_ROOM_MAINMENU;
		itemIndex = 0;

		std::cout << "Initializing SDL Game Controller" << std::endl;
		if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
		{
			std::cerr << "Could not initialize SDL: " << SDL_GetError() << std::endl;
			return;
		}
		
		// Init game controller
		gc = nullptr;
		for (int i = 0; i < SDL_NumJoysticks(); ++i)
		{
			if (SDL_IsGameController(i))
			{
				gc = SDL_GameControllerOpen(i);
				if (gc)
					break;
				else
				std::cerr << "Could not open Game Controller " << i << ": " << SDL_GetError() << std::endl;
			}
		}

		if(gc)
			std::cout << "Game Controller found and initialized" << std::endl;
		else
			std::cout << "No Game Controller found" << std::endl;
	}

	void GameController::process_events()
	{
		while(SDL_PollEvent(&e))
		{
			switch(e.type)
			{
				case SDL_CONTROLLERAXISMOTION:
				if(e.caxis.axis == 0 || e.caxis.axis == 1)
				{
					float tmp_axis[2];
					tmp_axis[e.caxis.axis] = clamp((float)std::round(e.caxis.value / (32767 / 2)), -1.0f, 1.0f);
					
					if(tmp_axis[0] != axis[0])
					{
						if (tmp_axis[0] == -1.0f)
						{
							handle_button(buttons::GC_BUTTON_LEFT, SDL_PRESSED);
							std::cout << "LEFT" << std::endl;
						}
						else if (tmp_axis[0] == 1.0f)
						{
							handle_button(buttons::GC_BUTTON_RIGHT, SDL_PRESSED);
							std::cout << "RIGHT" << std::endl;
						}
					}
					if(tmp_axis[1] != axis[1])
					{
						if (tmp_axis[1] == -1.0f)
						{
							handle_button(buttons::GC_BUTTON_UP, SDL_PRESSED);
							std::cout << "UP" << std::endl;
						}
						else if (tmp_axis[1] == 1.0f)
						{
							handle_button(buttons::GC_BUTTON_DOWN, SDL_PRESSED);
							std::cout << "DOWN" << std::endl;
						}
					}
					// Releases are not sent because we don't need them and also is a pain in the rear to handle them correctly.

					axis[e.caxis.axis] = clamp((float)std::round(e.caxis.value / (32767 / 2)), -1.0f, 1.0f);
				}
				break;
				case SDL_CONTROLLERBUTTONDOWN:
				case SDL_CONTROLLERBUTTONUP:
					if(e.cbutton.button == 0 || e.cbutton.button == 1)
						handle_button((ygo::buttons)e.cbutton.button, e.cbutton.state);
				break;
			}
		}
	}

	void GameController::end()
	{
		std::cout << "Closing Game Controller" << std::endl;
		SDL_Quit();
	}


	void GameController::update_room()
	{
		ygo::rooms tmp_room = croom;

		if(game->wMainMenu->isVisible())
			croom = rooms::GC_ROOM_MAINMENU;
		else if(game->wLanWindow->isVisible())
			croom = rooms::GC_ROOM_LAN_CONNECT;
		else if(game->wHostPrepare->isVisible())
			croom = rooms::GC_ROOM_LAN_SELECT_DECK;
		else if(game->wSinglePlay->isVisible())
			croom = rooms::GC_ROOM_PUZZLE;
		else if(game->wReplay->isVisible())
			croom = rooms::GC_ROOM_REPLAY;
		else if(game->is_building)
			croom = rooms::GC_ROOM_BUILDING;
		else if(game->dInfo.isStarted)
			croom = rooms::GC_ROOM_DUELING;
		else if(game->is_siding)
			croom = rooms::GC_ROOM_SIDING;

		if(tmp_room != croom)
			itemIndex = 0;
	}

	void GameController::handle_button(ygo::buttons b, int state)
	{
		update_room();
		switch(croom)
		{
		case rooms::GC_ROOM_MAINMENU:
			if(state == SDL_PRESSED)
			switch(b)
			{
				case buttons::GC_BUTTON_FORWARD:
					if(itemIndex == 0)
						ClickButton(game->btnLanMode);
					else if(itemIndex == 1)
						ClickButton(game->btnServerMode);
					else if(itemIndex == 2)
						ClickButton(game->btnReplayMode);
					else if(itemIndex == 3)
						ClickButton(game->btnDeckEdit);
					else if(itemIndex == 4)
						ClickButton(game->btnModeExit);
				break;

				case buttons::GC_BUTTON_UP: if(itemIndex > 0) itemIndex--; break;
				case buttons::GC_BUTTON_DOWN: if(itemIndex < 4) itemIndex++; break;
			}
		break;
		case rooms::GC_ROOM_LAN_CONNECT:
			if(state == SDL_PRESSED)
			switch(b)
			{
				case buttons::GC_BUTTON_FORWARD:
					ClickButton(game->btnJoinHost);
				break;
				case buttons::GC_BUTTON_BACKWARD:
					ClickButton(game->btnJoinCancel);
				break;
			}
		break;
		case rooms::GC_ROOM_LAN_SELECT_DECK:
			if(state == SDL_PRESSED)
			switch(b)
			{
				case buttons::GC_BUTTON_FORWARD:
					//ClickButton(game->btn);
				break;
				case buttons::GC_BUTTON_BACKWARD:
					ClickButton(game->btnHostPrepCancel);
				break;
			}
		break;
		case rooms::GC_ROOM_PUZZLE:
			if(state == SDL_PRESSED)
			switch(b)
			{
				case buttons::GC_BUTTON_BACKWARD:
					ClickButton(game->btnSinglePlayCancel);
				break;
			}
		break;
		case rooms::GC_ROOM_REPLAY:
			if(state == SDL_PRESSED)
			switch(b)
			{
				case buttons::GC_BUTTON_BACKWARD:
					ClickButton(game->btnReplayCancel);
				break;
			}
		break;
		case rooms::GC_ROOM_BUILDING:
			if(state == SDL_PRESSED)
			switch(b)
			{
				case buttons::GC_BUTTON_BACKWARD:
					ClickButton(game->btnLeaveGame);
				break;
			}
		break;
		case rooms::GC_ROOM_DUELING:
			if(state == SDL_PRESSED)
			switch(b)
			{
				case buttons::GC_BUTTON_FORWARD:
					game->dField.Clear();
				break;
				case buttons::GC_BUTTON_BACKWARD:
					ClickButton(game->btnLeaveGame);
				break;
			}
		break;
		}
	}
}
