#ifndef SOUND_IRRKLANG_H
#define SOUND_IRRKLANG_H
#include "../../sound_backend.h"
#include "irrklang_dynamic_loader.h"
#include <vector>
#include <string>
namespace irrklang {
class ISoundEngine;
class ISound;
}

class SoundIrrklang final : public SoundBackend {
public:
	SoundIrrklang();
	~SoundIrrklang() override;
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
	KlangLoader loader;
	irrklang::ISoundEngine* soundEngine;
	irrklang::ISound* soundBGM;
	std::vector<irrklang::ISound*> sounds;
	double sfxVolume;
	double bgmVolume;
};

#endif //SOUND_IRRKLANG_H
