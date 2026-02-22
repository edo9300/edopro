#ifdef YGOPRO_USE_SDL_MIXER3
#include "sound_sdlmixer3.h"
#include "../../fmt.h"
#include <stdexcept>
#include <SDL3_mixer/SDL_mixer.h>
#include "../../epro_thread.h"
#include <atomic>

SoundMixer3Base::SoundMixer3Base() : mixer(nullptr), sound_volume(1.0f), loop_properties(0) {
	static_assert(sizeof(loop_properties) >= sizeof(SDL_PropertiesID));
	if(!MIX_Init()) {
		throw std::runtime_error(epro::format("Failed to init\n{}", SDL_GetError()));
	}
	auto number_of_decoders = MIX_GetNumAudioDecoders();
	epro::print("SDL3 Mixer decoders:\n");
	for(int i = 0; i < number_of_decoders; ++i) {
		epro::print("{}\n", MIX_GetAudioDecoder(i));
	}
	mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
	if(mixer == nullptr) {
		MIX_Quit();
		throw std::runtime_error(epro::format("Cannot open channels\nMix_OpenAudio: {}\n", SDL_GetError()));
	}
	music_track = MIX_CreateTrack(mixer);
	if(mixer == nullptr) {
		MIX_DestroyMixer(mixer);
		MIX_Quit();
		throw std::runtime_error(epro::format("Cannot create music track\nMIX_CreateTrack: {}\n", SDL_GetError()));
	}
	loop_properties = SDL_CreateProperties();
	SDL_SetNumberProperty(loop_properties, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
}
void SoundMixer3Base::SetSoundVolume(double volume) {
	sound_volume = volume;
	MIX_SetTagGain(mixer, "sound", volume);
}
void SoundMixer3Base::SetMusicVolume(double volume) {
	MIX_SetTrackGain(music_track, volume);
}
bool SoundMixer3Base::PlayMusic(const std::string& name, bool loop) {
	if(cur_music == name)
		return false;
	MIX_StopTrack(music_track, 0);
	MIX_SetTrackAudio(music_track, nullptr);
	auto* io = SDL_IOFromFile(name.data(), "rb");
	if(!io) {
		return false;
	}
	if(!MIX_SetTrackIOStream(music_track, io, true)) {
		return false;
	}
	SDL_PropertiesID props = loop ? static_cast<SDL_PropertiesID>(loop_properties) : 0;
	if(!MIX_PlayTrack(music_track, props)) {
		MIX_SetTrackAudio(music_track, nullptr);
		return false;
	}
	return true;
}
MIX_Audio* SoundMixer3Base::getCachedSound(const std::string& path) {
	auto it = cached_sounds.find(path);
	if(it != cached_sounds.end())
		return it->second;
	auto* audio = MIX_LoadAudio(mixer, path.data(), false);
	if(!audio) {
		return nullptr;
	}
	cached_sounds.emplace(path, audio);
	MIX_DestroyAudio(audio);
}
MIX_Track* SoundMixer3Base::createAudioTrack(const std::string& path) {
	auto* sound = getCachedSound(path);
	if(!sound) {
		return nullptr;
	}
	auto* track = MIX_CreateTrack(mixer);
	if(!track) {
		return nullptr;
	}
	MIX_TagTrack(track, "sound");
	MIX_SetTrackAudio(track, sound);
	MIX_SetTrackGain(track, sound_volume);
	return track;
}
bool SoundMixer3Base::PlaySound(const std::string& name) {
	auto* chunk = createAudioTrack(name);
	if(!chunk || !MIX_PlayTrack(chunk, 0)) {
		return false;
	}
	playing_sounds.push_back(chunk);
	return true;
}
void SoundMixer3Base::StopSounds() {
	MIX_StopTag(mixer, "sound", 0);
	for(auto& track : playing_sounds) {
		MIX_DestroyTrack(track);
	}
	playing_sounds.clear();
}
void SoundMixer3Base::StopMusic() {
	MIX_StopTrack(music_track, 0);
	MIX_SetTrackAudio(music_track, nullptr);
}
void SoundMixer3Base::PauseMusic(bool pause) {
	if(!pause) {
		MIX_PauseTrack(music_track);
	} else {
		MIX_ResumeTrack(music_track);
	}
}
void SoundMixer3Base::LoopMusic(bool loop) {
	if(!MusicPlaying())
		return;
	MIX_SetTrackLoops(music_track, loop ? -1 : 0);
}
bool SoundMixer3Base::MusicPlaying() {
	return MIX_GetTrackRemaining(music_track) != 0;
}
void SoundMixer3Base::Tick() {
	for(auto it = playing_sounds.begin(); it != playing_sounds.end();) {
		if(MIX_GetTrackRemaining(*it) == 0) {
			MIX_DestroyTrack(*it);
			it = playing_sounds.erase(it);
		} else
			it++;
	}
}
//In some occasions Mix_Quit can get stuck and never return, use this as failsafe
static void KillSwitch(std::weak_ptr<bool> die) {
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
	if(auto val = die.lock(); val)
		exit(0);
}
SoundMixer3Base::~SoundMixer3Base() {
	auto die = std::make_shared<bool>(true);
	epro::thread(KillSwitch, std::weak_ptr<bool>{die}).detach();
	StopMusic();
	StopSounds();
	SDL_DestroyProperties(loop_properties);
	MIX_DestroyTrack(music_track);
	MIX_DestroyMixer(mixer);
	MIX_Quit();
}

#endif //YGOPRO_USE_SDL_MIXER
