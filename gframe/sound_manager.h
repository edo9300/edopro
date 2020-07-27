#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <memory>
#include "random_fwd.h"
#include <map>
#include "text_types.h"
#include "sound_backend.h"

namespace ygo {

class SoundManager {
public:
	enum SFX {
		SUMMON,
		SPECIAL_SUMMON,
		ACTIVATE,
		SET,
		FLIP,
		REVEAL,
		EQUIP,
		DESTROYED,
		BANISHED,
		TOKEN,
		ATTACK,
		DIRECT_ATTACK,
		DRAW,
		SHUFFLE,
		DAMAGE,
		RECOVER,
		COUNTER_ADD,
		COUNTER_REMOVE,
		COIN,
		DICE,
		NEXT_TURN,
		PHASE,
		PLAYER_ENTER,
		CHAT
	};
	enum BGM {
		ALL,
		DUEL,
		MENU,
		DECK,
		ADVANTAGE,
		DISADVANTAGE,
		WIN,
		LOSE
	};
	enum class CHANT {
		SUMMON,
		ATTACK,
		ACTIVATE
	};
	//////kdiy/////
	enum CHANTSP {
		SETS,
		EQUIPS,
		DESTROYS,
		BANISHS,
		DRAWS,	
		DAMAGES,
		RECOVERS,
		NEXTTURNS
	};
	//////kdiy/////	
	SoundManager(double sounds_volume, double music_volume, bool sounds_enabled, bool music_enabled, const path_string& working_directory);
	bool IsUsable();
	void RefreshBGMList();
	void RefreshChantsList();
	//////kdiy//////
	void RefreshChantSPList();	
	bool PlayChantSP(CHANTSP scene);	
	//////kdiy//////	
	void PlaySoundEffect(SFX sound);
	void PlayBGM(BGM scene, bool loop = true);
	bool PlayChant(CHANT chant, unsigned int code);
	void SetSoundVolume(double volume);
	void SetMusicVolume(double volume);
	void EnableSounds(bool enable);
	void EnableMusic(bool enable);
	void StopSounds();
	void StopMusic();
	void PauseMusic(bool pause);
	void Tick();

private:
	std::vector<std::string> BGMList[8];
	std::map<std::pair<CHANT, unsigned int>, std::string> ChantsList;
	////////kdiy////
	std::vector<std::string> ChantSPList[8];	
	////////kdiy////	
	int bgm_scene = -1;
	randengine rnd;
	std::unique_ptr<SoundBackend> mixer;
	void RefreshBGMDir(path_string path, BGM scene);
	//////kdiy//////
	void RefreshSPDir(path_string path, CHANTSP scene);	
	//////kdiy//////	
	bool soundsEnabled = false;
	bool musicEnabled = false;
	std::string working_dir = "./";
	bool succesfully_initied = false;
};

extern SoundManager* gSoundManager;

}

#endif //SOUNDMANAGER_H
