#ifdef YGOPRO_USE_IRRKLANG
#include "sound_irrklang.h"
#include "irrklang_dynamic_loader.h"
#include <stdexcept>
#include <irrKlang.h>
#ifdef IRRKLANG_STATIC
#include "../ikpmp3/ikpMP3.h"
#endif

SoundIrrklang::SoundIrrklang() {
	irrdyn::init();
	soundEngine = irrdyn::createIrrKlangDevice();
	if(!soundEngine) {
		throw std::runtime_error("Failed to init irrklang device!");
	}
	soundBGM = nullptr;
#ifdef IRRKLANG_STATIC
	irrklang::ikpMP3Init(soundEngine);
#endif
}
void SoundIrrklang::SetSoundVolume(double volume) {
	sfxVolume = volume;
}
void SoundIrrklang::SetMusicVolume(double volume) {
	if(soundBGM)
		soundBGM->setVolume(volume);
	bgmVolume = volume;
}
bool SoundIrrklang::PlayMusic(const std::string& name, bool loop) {
	if(!soundBGM || soundBGM->getSoundSource()->getName() != name) {
		StopMusic();
		if(soundEngine) {
			soundBGM = soundEngine->play2D(name.c_str(), loop, true, true);
			soundBGM->setVolume(bgmVolume);
			soundBGM->setIsPaused(false);
		}
	}
	return true;
}
bool SoundIrrklang::PlaySound(const std::string& name) {
	auto sfx = soundEngine->play2D(name.c_str(), false, true);
	if(!sfx)
		return false;
	sfx->setVolume(sfxVolume);
	sfx->setIsPaused(false);
	sounds.push_back(sfx);
	return true;
}
void SoundIrrklang::StopSounds() {
	for(auto& sound : sounds)
		sound->stop();
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
	if(soundEngine)
		soundEngine->drop();
	irrdyn::close();
}

#endif //YGOPRO_USE_IRRKLANG