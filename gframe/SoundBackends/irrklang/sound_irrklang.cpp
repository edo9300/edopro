#ifdef YGOPRO_USE_IRRKLANG
#include "sound_irrklang.h"

#include <stdexcept>
#include <string>
#include <vector>

#include <irrKlang.h>

#include "irrklang_dynamic_loader.h"
#include "../../sound_backend.h"

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

template<>
std::unique_ptr<SoundBackend> SoundBackendHelper<SoundIrrklang>::make_ptr() {
	return std::make_unique<SoundIrrklang>();
}

SoundIrrklang::SoundIrrklang() :
	soundEngine(nullptr), soundBGM(nullptr), sfxVolume(0.0), bgmVolume(0.0) {
	if(!(soundEngine = loader.createIrrKlangDevice()))
		throw std::runtime_error("Failed to init irrklang device!");
}
void SoundIrrklang::SetSoundVolume(double volume) {
	sfxVolume = volume;
}
void SoundIrrklang::SetMusicVolume(double volume) {
	if(soundBGM)
		soundBGM->setVolume((irrklang::ik_f32)volume);
	bgmVolume = volume;
}
bool SoundIrrklang::PlayMusic(const std::string& name, bool loop) {
	if(!soundBGM || soundBGM->getSoundSource()->getName() != name) {
		StopMusic();
		if(soundEngine) {
			soundBGM = soundEngine->play2D(name.data(), loop, true, true);
			if(!soundBGM)
				return false;
			soundBGM->setVolume((irrklang::ik_f32)bgmVolume);
			soundBGM->setIsPaused(false);
		}
	}
	return true;
}
bool SoundIrrklang::PlaySound(const std::string& name) {
	auto sfx = soundEngine->play2D(name.data(), false, true);
	if(!sfx)
		return false;
	sfx->setVolume((irrklang::ik_f32)sfxVolume);
	sfx->setIsPaused(false);
	sounds.push_back(sfx);
	return true;
}
void SoundIrrklang::StopSounds() {
	for(auto& sound : sounds) {
		sound->stop();
		sound->drop();
	}
	sounds.clear();
}
void SoundIrrklang::StopMusic() {
	if(soundBGM) {
		soundBGM->stop();
		soundBGM->drop();
		soundBGM = nullptr;
	}
}
void SoundIrrklang::PauseMusic(bool pause) {
	if(soundBGM)
		soundBGM->setIsPaused(pause);
}
void SoundIrrklang::LoopMusic(bool loop) {
	if(soundBGM)
		soundBGM->setIsLooped(loop);
}
bool SoundIrrklang::MusicPlaying() {
	return soundBGM && !soundBGM->isFinished();
}
void SoundIrrklang::Tick() {
	for(auto itsound = sounds.begin(); itsound != sounds.end();) {
		auto sound = *itsound;
		if(sound->isFinished()) {
			sound->drop();
			itsound = sounds.erase(itsound);
		} else {
			itsound++;
		}
	}
}
SoundIrrklang::~SoundIrrklang() {
	if(soundBGM)
		soundBGM->drop();
	StopSounds();
	if(soundEngine)
		soundEngine->drop();
}

#endif //YGOPRO_USE_IRRKLANG
