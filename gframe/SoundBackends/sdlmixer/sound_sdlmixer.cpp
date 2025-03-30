#ifdef YGOPRO_USE_SDL_MIXER
#include "sound_sdlmixer.h"
#include "../../fmt.h"
#define SDL_MAIN_HANDLED
#include <stdexcept>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../../epro_thread.h"
#include <atomic>

SoundMixerBase::SoundMixerBase() : music(nullptr), sound_volume(0), music_volume(0) {
	SDL_SetMainReady();
	if(SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
		throw std::runtime_error(epro::format("Failed to init sdl audio device!\nSDL_InitSubSystem: {}\n", SDL_GetError()));
	int flags = MIX_INIT_OGG | MIX_INIT_MP3 | MIX_INIT_FLAC;
	int initted = Mix_Init(flags);
	epro::print("MIX_INIT_OGG: {}\n", !!(initted & MIX_INIT_OGG));
	epro::print("MIX_INIT_MP3: {}\n", !!(initted & MIX_INIT_MP3));
	epro::print("MIX_INIT_FLAC: {}\n", !!(initted & MIX_INIT_FLAC));
	if((initted&flags) != flags) {
		Mix_Quit();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		throw std::runtime_error("Not all flags set");
	}
	if(Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 4096) == -1) {
		Mix_Quit();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		throw std::runtime_error(epro::format("Cannot open channels\nMix_OpenAudio: {}\n", SDL_GetError()));
	}
}
void SoundMixerBase::SetSoundVolume(double volume) {
	sound_volume = (int)(volume * 128);
	Mix_Volume(-1, sound_volume);
}
void SoundMixerBase::SetMusicVolume(double volume) {
	music_volume = (int)(volume * 128);
	Mix_VolumeMusic(music_volume);
}
bool SoundMixerBase::PlayMusic(const std::string& name, bool loop) {
	if(music && cur_music == name)
		return false;
	if(music) {
		Mix_HaltMusic();
		Mix_FreeMusic(music);
	}
	music = Mix_LoadMUS(name.data());
	if(music) {
		if(Mix_PlayMusic(music, loop ? -1 : 0) == -1) {
			Mix_FreeMusic(music);
			return false;
		} else {
			cur_music = name;
		}
	} else {
		return false;
	}
	return true;
}
bool SoundMixerBase::PlaySound(const std::string& name) {
	auto chunk = Mix_LoadWAV(name.data());
	if(chunk) {
		auto channel = Mix_PlayChannel(-1, chunk, 0);
		if(channel == -1) {
			Mix_FreeChunk(chunk);
			return false;
		}
		sounds[channel] = chunk;
		Mix_Volume(-1, sound_volume);
	} else {
		return false;
	}
	return true;
}
void SoundMixerBase::StopSounds() {
	Mix_HaltChannel(-1);
	for(auto& chunk : sounds) {
		if(chunk.second)
			Mix_FreeChunk(chunk.second);
	}
	sounds.clear();
}
void SoundMixerBase::StopMusic() {
	if(music) {
		Mix_HaltMusic();
		Mix_FreeMusic(music);
		music = nullptr;
	}
}
void SoundMixerBase::PauseMusic(bool pause) {
	if(!music)
		return;
	if(pause)
		Mix_PauseMusic();
	else
		Mix_ResumeMusic();
}
void SoundMixerBase::LoopMusic(bool loop) {
	if(!MusicPlaying())
		return;
	// ugly, but wathever
	Mix_PauseMusic();
	auto position = Mix_GetMusicPosition(music);
	Mix_PlayMusic(music, loop ? -1 : 0);
	Mix_SetMusicPosition(position);
}
bool SoundMixerBase::MusicPlaying() {
	return Mix_PlayingMusic();
}
void SoundMixerBase::Tick() {
	for(auto chunk = sounds.begin(); chunk != sounds.end();) {
		if(Mix_Playing(chunk->first) == 0) {
			Mix_FreeChunk(chunk->second);
			sounds.erase(chunk++);
		} else
			chunk++;
	}
}
//In some occasions Mix_Quit can get stuck and never return, use this as failsafe
static void KillSwitch(std::weak_ptr<bool> die) {
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	if(auto val = die.lock(); val)
		exit(0);
}
SoundMixerBase::~SoundMixerBase() {
	auto die = std::make_shared<bool>(true);
	epro::thread(KillSwitch, std::weak_ptr<bool>{die}).detach();
	StopMusic();
	StopSounds();
	Mix_CloseAudio();
	Mix_Quit();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

#endif //YGOPRO_USE_SDL_MIXER
