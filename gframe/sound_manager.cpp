#include "sound_manager.h"
#include "config.h"
#ifdef IRRKLANG_STATIC
#include "../ikpmp3/ikpMP3.h"
#endif

namespace ygo {

SoundManager soundManager;

#ifndef YGOPRO_USE_IRRKLANG
/* Modified from minetest: src/client/sound_openal.cpp 
 * https://github.com/minetest/minetest/blob/master/src/client/sound_openal.cpp
 * Licensed under GNU LGPLv2.1
 */
static void delete_ALCdevice(ALCdevice* ptr)
{
    if (ptr) {
        alcCloseDevice(ptr);
    }
}
static void delete_ALCcontext(ALCcontext* ptr)
{
    if (ptr) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(ptr);
    }
}
SoundManager::SoundManager() : device(nullptr, delete_ALCdevice), context(nullptr, delete_ALCcontext) {}
#endif
bool SoundManager::Init(double sounds_volume, double music_volume, bool sounds_enabled, bool music_enabled, void* payload) {
	soundsEnabled = sounds_enabled;
	musicEnabled = music_enabled;
    bgm_scene = BGM::NONE;
    RefreshBGMList();
    RefreshChantsList();
#ifdef YGOPRO_USE_IRRKLANG
	rnd.seed(time(0));
	soundEngine = irrklang::createIrrKlangDevice();
	if(!soundEngine) {
		return false;
	} else {
#ifdef IRRKLANG_STATIC
		irrklang::ikpMP3Init(soundEngine);
#endif
        soundEngine->setSoundVolume(sounds_volume);
        return true;
	}
#else
    device.reset(alcOpenDevice(nullptr));
    if (!device) {
        // OUTPUT ERROR: Failed to create audio device!
        return false;
    }
    context.reset(alcCreateContext(device.get(), nullptr));
    if (!alcMakeContextCurrent(context.get())) {
        // OUTPUT ERROR: Failed to set default context!
        return false;
    }
    ALenum error;
#define RETURN_ON_ERROR(message) \
    if ((error = alGetError()) != AL_NO_ERROR) { \
        return false; \
    }
    alGenBuffers(1, &bufferSfx);
    RETURN_ON_ERROR(0)
    alGenBuffers(1, &bufferBgm);
    RETURN_ON_ERROR(0)
    alGenSources(1, &sourceSfx);
    RETURN_ON_ERROR(0)
    alSourcei(sourceSfx, AL_BUFFER, bufferSfx);
    RETURN_ON_ERROR(0)
    alGenSources(1, &sourceBgm);
    RETURN_ON_ERROR(0)
    alSourcei(sourceBgm, AL_BUFFER, bufferBgm);
    RETURN_ON_ERROR(0)
#undef RETURN_ON_ERROR
    return true;
#endif // YGOPRO_USE_IRRKLANG
}
SoundManager::~SoundManager() {
#ifdef YGOPRO_USE_IRRKLANG
    if (soundBGM)
        soundBGM->drop();
    if (soundEngine)
        soundEngine->drop();
#else
    alDeleteSources(1, &sourceBgm);
    alDeleteSources(1, &sourceSfx);
    alDeleteBuffers(1, &bufferBgm);
    alDeleteBuffers(1, &bufferSfx);
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context.get());
    alcCloseDevice(device.get());
#endif
}
void SoundManager::RefreshBGMList() {
	Utils::Makedirectory(TEXT("./sound/BGM/"));
	Utils::Makedirectory(TEXT("./sound/BGM/duel"));
	Utils::Makedirectory(TEXT("./sound/BGM/menu"));
	Utils::Makedirectory(TEXT("./sound/BGM/deck"));
	Utils::Makedirectory(TEXT("./sound/BGM/advantage"));
	Utils::Makedirectory(TEXT("./sound/BGM/disadvantage"));
	Utils::Makedirectory(TEXT("./sound/BGM/win"));
	Utils::Makedirectory(TEXT("./sound/BGM/lose"));
	Utils::Makedirectory(TEXT("./sound/chants"));
	RefreshBGMDir(TEXT(""), BGM::DUEL);
	RefreshBGMDir(TEXT("duel"), BGM::DUEL);
	RefreshBGMDir(TEXT("menu"), BGM::MENU);
	RefreshBGMDir(TEXT("deck"), BGM::DECK);
	RefreshBGMDir(TEXT("advantage"), BGM::ADVANTAGE);
	RefreshBGMDir(TEXT("disadvantage"), BGM::DISADVANTAGE);
	RefreshBGMDir(TEXT("win"), BGM::WIN);
	RefreshBGMDir(TEXT("lose"), BGM::LOSE);
}
void SoundManager::RefreshBGMDir(path_string path, BGM scene) {
	for(auto& file : Utils::FindfolderFiles(TEXT("./sound/BGM/") + path, { TEXT("mp3"), TEXT("ogg"), TEXT("wav") })) {
		auto conv = Utils::ToUTF8IfNeeded(path + TEXT("/") + file);
		BGMList[BGM::ALL].push_back(conv);
		BGMList[scene].push_back(conv);
	}
}
void SoundManager::RefreshChantsList() {
	for(auto& file : Utils::FindfolderFiles(TEXT("./sound/chants"), { TEXT("mp3"), TEXT("ogg"), TEXT("wav") })) {
		auto scode = Utils::GetFileName(TEXT("./sound/chants/") + file);
		unsigned int code = std::stoi(scode);
		if(code && !ChantsList.count(code))
			ChantsList[code] = Utils::ToUTF8IfNeeded(file);
	}
}
void SoundManager::PlaySoundEffect(SFX sound) {
    static const std::map<SFX, const char*> fx = {
        {SUMMON, "./sound/summon.wav"},
        {SPECIAL_SUMMON, "./sound/specialsummon.wav"},
        {ACTIVATE, "./sound/activate.wav"},
        {SET, "./sound/set.wav"},
        {FLIP, "./sound/flip.wav"},
        {REVEAL, "./sound/reveal.wav"},
        {EQUIP, "./sound/equip.wav"},
        {DESTROYED, "./sound/destroyed.wav"},
        {BANISHED, "./sound/banished.wav"},
        {TOKEN, "./sound/token.wav"},
        {ATTACK, "./sound/attack.wav"},
        {DIRECT_ATTACK, "./sound/directattack.wav"},
        {DRAW, "./sound/draw.wav"},
        {SHUFFLE, "./sound/shuffle.wav"},
        {DAMAGE, "./sound/damage.wav"},
        {RECOVER, "./sound/gainlp.wav"},
        {COUNTER_ADD, "./sound/addcounter.wav"},
        {COUNTER_REMOVE, "./sound/removecounter.wav"},
        {COIN, "./sound/coinflip.wav"},
        {DICE, "./sound/diceroll.wav"},
        {NEXT_TURN, "./sound/nextturn.wav"},
        {PHASE, "./sound/phase.wav"},
        {PLAYER_ENTER, "./sound/playerenter.wav"},
        {CHAT, "./sound/chatmessage.wav"}
    };
    if (!soundsEnabled) return;
    if (fx.find(sound) != fx.end()) {
#ifdef YGOPRO_USE_IRRKLANG
        soundEngine->play2D(fx.at(sound));
#endif
    }
}
void SoundManager::PlayMusic(const std::string& song, bool loop) {
	if(!musicEnabled) return;
#ifdef YGOPRO_USE_IRRKLANG
	if(!soundBGM || soundBGM->getSoundSource()->getName() != song) {
		if(soundBGM) {
			soundBGM->stop();
			soundBGM->drop();
			soundBGM = nullptr;
		}
		soundBGM = soundEngine->play2D(song.c_str(), loop, false, true);
	}
#endif
}
void SoundManager::PlayBGM(BGM scene) {
#ifdef YGOPRO_USE_IRRKLANG
	auto& list = BGMList[scene];
	int count = list.size();
	if(musicEnabled && (scene != bgm_scene || (soundBGM && soundBGM->isFinished()) || !soundBGM) && count > 0) {
		bgm_scene = scene;
		int bgm = (std::uniform_int_distribution<>(0, count - 1))(rnd);
		std::string BGMName = "./sound/BGM/" + list[bgm];
		PlayMusic(BGMName, true);
	}
#endif
}
void SoundManager::StopBGM() {
#ifdef YGOPRO_USE_IRRKLANG
	if(soundBGM) {
		soundBGM->stop();
		soundBGM->drop();
		soundBGM = nullptr;
	}
#else
    alSourceStop(sourceBgm);
#endif
}
bool SoundManager::PlayChant(unsigned int code) {
#ifdef YGOPRO_USE_IRRKLANG
	if(ChantsList.count(code)) {
		if(!soundEngine->isCurrentlyPlaying(("./sound/chants/" + ChantsList[code]).c_str()))
			soundEngine->play2D(("./sound/chants/" + ChantsList[code]).c_str());
		return true;
	}
#endif
	return false;
}
void SoundManager::SetSoundVolume(double volume) {
#ifdef YGOPRO_USE_IRRKLANG
	soundEngine->setSoundVolume(volume);
#else
    alSourcef(sourceSfx, AL_GAIN, volume);
#endif
}
void SoundManager::SetMusicVolume(double volume) {
#ifdef YGOPRO_USE_IRRKLANG
	soundEngine->setSoundVolume(volume);
#else
    alSourcef(sourceBgm, AL_GAIN, volume);
#endif
}
void SoundManager::EnableSounds(bool enable) {
	soundsEnabled = enable;
}
void SoundManager::EnableMusic(bool enable) {
	musicEnabled = enable;
	if(!musicEnabled) {
#ifdef YGOPRO_USE_IRRKLANG
		if(soundBGM){
			if(!soundBGM->isFinished())
				soundBGM->stop();
			soundBGM->drop();
			soundBGM = nullptr;
		}
#else
        alSourceStop(sourceBgm);
#endif
	}
}

} // namespace ygo
