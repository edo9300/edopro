#ifndef SOUND_SDL_MIXER_H
#define SOUND_SDL_MIXER_H
#include "../sound_threaded_backend.h"
#include "../../sound_backend.h"
#include <map>
#include <string>
struct _Mix_Music;
typedef struct _Mix_Music Mix_Music;
struct Mix_Chunk;

class SoundMixerBase final : public SoundBackend {
public:
	SoundMixerBase();
	~SoundMixerBase() override;
	void SetSoundVolume(double volume) override;
	void SetMusicVolume(double volume) override;
	bool PlayMusic(const std::string& name, bool loop) override;
	bool PlaySound(const std::string& name) override;
	void StopSounds() override;
	void StopMusic() override;
	void PauseMusic(bool pause) override;
	bool MusicPlaying() override;
	void Tick() override;
private:
	std::string cur_music;
	std::map<int, Mix_Chunk*> sounds;
	Mix_Music* music;
	int sound_volume, music_volume;
};

using SoundMixer = SoundThreadedBackendHelper<SoundMixerBase>;

#endif //SOUND_SDL_MIXER_H
