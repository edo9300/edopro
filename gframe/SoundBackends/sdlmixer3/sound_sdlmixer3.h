#ifndef SOUND_SDL_MIXER3_H
#define SOUND_SDL_MIXER3_H
#include "../sound_threaded_backend.h"
#include "../../sound_backend.h"
#include <map>
#include <string>
struct MIX_Audio;
typedef struct MIX_Audio MIX_Audio;
struct MIX_Track;
typedef struct MIX_Track MIX_Track;

struct MIX_Mixer;
typedef struct MIX_Mixer MIX_Mixer;

class SoundMixer3Base final : public SoundBackend {
public:
	SoundMixer3Base();
	~SoundMixer3Base() override;
	void SetSoundVolume(double volume) override;
	void SetMusicVolume(double volume) override;
	bool PlayMusic(const std::string& name, bool loop) override;
	bool PlaySound(const std::string& name) override;
	void StopSounds() override;
	void StopMusic() override;
	void PauseMusic(bool pause) override;
	void LoopMusic(bool loop) override;
	bool MusicPlaying() override;
	void Tick() override;
private:
	MIX_Audio* getCachedSound(const std::string& path);
	MIX_Track* createAudioTrack(const std::string& path);
	std::string cur_music;
	std::map<std::string, MIX_Audio*> cached_sounds;
	std::vector<MIX_Track*> playing_sounds;
	MIX_Track* music_track;
	MIX_Mixer* mixer;
	float sound_volume;
	uint64_t loop_properties;
};

using SoundMixer3 = SoundThreadedBackendHelper<SoundMixer3Base>;

#endif //SOUND_SDL_MIXER_H
