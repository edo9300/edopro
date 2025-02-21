#ifndef SOUND_MINIAUDIO_H
#define SOUND_MINIAUDIO_H
#include "../sound_threaded_backend.h"
#include "../../sound_backend.h"
#include <memory>
#include <string>
#include <vector>

class MaEngine;
class MaSound;
class MaSoundGroup;

class SoundMiniaudioBase final : public SoundBackend {
public:
	SoundMiniaudioBase();
	~SoundMiniaudioBase() override;
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
	static void FreeEngine(MaEngine* engine);
	static void FreeSound(MaSound* sound);
	static void FreeSoundGroup(MaSoundGroup* sound_group);
	using EnginePtr = std::unique_ptr<MaEngine, decltype(&FreeEngine)>;
	using SoundPtr = std::unique_ptr<MaSound, decltype(&FreeSound)>;
	using SoundGroupPtr = std::unique_ptr<MaSoundGroup, decltype(&FreeSoundGroup)>;
	SoundPtr openSound(const std::string& name, bool isMusic);
	std::string cur_music;
	EnginePtr engine;
	SoundGroupPtr sounds_group;
	std::vector<SoundPtr> sounds;
	SoundPtr music;
	float sound_volume, music_volume;
};

using SoundMiniaudio = SoundThreadedBackendHelper<SoundMiniaudioBase>;

#endif //SOUND_MINIAUDIO_H
