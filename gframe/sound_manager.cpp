#define WIN32_LEAN_AND_MEAN
#include "sound_manager.h"
#include "utils.h"
#include "config.h"
#if defined(YGOPRO_USE_IRRKLANG)
#include "sound_irrklang.h"
#define BACKEND SoundIrrklang
#elif defined(YGOPRO_USE_SDL_MIXER)
#include "sound_sdlmixer.h"
#define BACKEND SoundMixer
#elif defined(YGOPRO_USE_SFML)
#include "sound_sfml.h"
#define BACKEND SoundSFML
#endif
/////kdiy/////////
#include "game_config.h"
/////kdiy/////////

namespace ygo {
SoundManager::SoundManager(double sounds_volume, double music_volume, bool sounds_enabled, bool music_enabled, epro::path_stringview working_directory) {
#ifdef BACKEND
	fmt::print("Using: " STR(BACKEND)" for audio playback.\n");
	working_dir = Utils::ToUTF8IfNeeded(working_directory);
	soundsEnabled = sounds_enabled;
	musicEnabled = music_enabled;
	try {
		mixer = std::unique_ptr<SoundBackend>(new BACKEND());
		mixer->SetMusicVolume(music_volume);
		mixer->SetSoundVolume(sounds_volume);
	}
	catch(const std::runtime_error& e) {
		fmt::print("Failed to initialize audio backend:\n");
		fmt::print(e.what());
		succesfully_initied = soundsEnabled = musicEnabled = false;
		return;
	}
	catch(...) {
		fmt::print("Failed to initialize audio backend.\n");
		succesfully_initied = soundsEnabled = musicEnabled = false;
		return;
	}
	rnd.seed(time(0)&0xffffffff);
	////////kdiy////
	std::string bgm_now = "";
	////////kdiy////
	bgm_scene = -1;
	RefreshBGMList();
	RefreshSoundsList();
	RefreshChantsList();
	succesfully_initied = true;
#else
	fmt::print("No audio backend available.\nAudio will be disabled.\n");
	succesfully_initied = soundsEnabled = musicEnabled = false;
	return;
#endif // BACKEND
}
bool SoundManager::IsUsable() {
	return succesfully_initied;
}
void SoundManager::RefreshBGMList() {
#ifdef BACKEND
    ////////kdiy////
	Utils::MakeDirectory(EPRO_TEXT("./sound/character/player"));
    Utils::MakeDirectory(EPRO_TEXT("./sound/character/atem"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/character/kaiba"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/character/joey"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/character/donthousand"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/character/shark"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/character/yuma"));
	////////kdiy////
	Utils::MakeDirectory(EPRO_TEXT("./sound/BGM/"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/BGM/duel"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/BGM/menu"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/BGM/deck"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/BGM/advantage"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/BGM/disadvantage"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/BGM/win"));
	Utils::MakeDirectory(EPRO_TEXT("./sound/BGM/lose"));
	for (auto& list : BGMList)
		list.clear();
	RefreshBGMDir(EPRO_TEXT(""), BGM::DUEL);
	RefreshBGMDir(EPRO_TEXT("duel"), BGM::DUEL);
	RefreshBGMDir(EPRO_TEXT("menu"), BGM::MENU);
	RefreshBGMDir(EPRO_TEXT("deck"), BGM::DECK);
	RefreshBGMDir(EPRO_TEXT("advantage"), BGM::ADVANTAGE);
	RefreshBGMDir(EPRO_TEXT("disadvantage"), BGM::DISADVANTAGE);
	RefreshBGMDir(EPRO_TEXT("win"), BGM::WIN);
	RefreshBGMDir(EPRO_TEXT("lose"), BGM::LOSE);
#endif
}
void SoundManager::RefreshSoundsList() {
#if defined(_MSC_VER) && _MSC_VER == 1900
	static const std::pair<SFX, epro::path_stringview> fx[] = {
#else
	static constexpr std::pair<SFX, epro::path_stringview> fx[] = {
#endif
		{SUMMON, EPRO_TEXT("./sound/summon.{}")},
		{SPECIAL_SUMMON, EPRO_TEXT("./sound/specialsummon.{}")},
		{ACTIVATE, EPRO_TEXT("./sound/activate.{}")},
		{SET, EPRO_TEXT("./sound/set.{}")},
		{FLIP, EPRO_TEXT("./sound/flip.{}")},
		{REVEAL, EPRO_TEXT("./sound/reveal.{}")},
		{EQUIP, EPRO_TEXT("./sound/equip.{}")},
		{DESTROYED, EPRO_TEXT("./sound/destroyed.{}")},
		{BANISHED, EPRO_TEXT("./sound/banished.{}")},
		{TOKEN, EPRO_TEXT("./sound/token.{}")},
		{ATTACK, EPRO_TEXT("./sound/attack.{}")},
		{DIRECT_ATTACK, EPRO_TEXT("./sound/directattack.{}")},
		{DRAW, EPRO_TEXT("./sound/draw.{}")},
		{SHUFFLE, EPRO_TEXT("./sound/shuffle.{}")},
		{DAMAGE, EPRO_TEXT("./sound/damage.{}")},
		{RECOVER, EPRO_TEXT("./sound/gainlp.{}")},
		{COUNTER_ADD, EPRO_TEXT("./sound/addcounter.{}")},
		{COUNTER_REMOVE, EPRO_TEXT("./sound/removecounter.{}")},
		{COIN, EPRO_TEXT("./sound/coinflip.{}")},
		{DICE, EPRO_TEXT("./sound/diceroll.{}")},
		{NEXT_TURN, EPRO_TEXT("./sound/nextturn.{}")},
		{PHASE, EPRO_TEXT("./sound/phase.{}")},
		{PLAYER_ENTER, EPRO_TEXT("./sound/playerenter.{}")},
		{CHAT, EPRO_TEXT("./sound/chatmessage.{}")}
	};
	const auto extensions = mixer->GetSupportedSoundExtensions();
	for(const auto& sound : fx) {
		for(const auto& ext : extensions) {
			const auto filename = fmt::format(sound.second, ext);
			if(Utils::FileExists(filename)) {
				SFXList[sound.first] = Utils::ToUTF8IfNeeded(filename);
				break;
			}
		}
	}
}
void SoundManager::RefreshBGMDir(epro::path_stringview path, BGM scene) {
#ifdef BACKEND
	for(auto& file : Utils::FindFiles(fmt::format(EPRO_TEXT("./sound/BGM/{}"), path), mixer->GetSupportedMusicExtensions())) {
		auto conv = Utils::ToUTF8IfNeeded(fmt::format(EPRO_TEXT("{}/{}"), path, file));
		BGMList[BGM::ALL].push_back(conv);
		BGMList[scene].push_back(std::move(conv));
	}
#endif
}
void SoundManager::RefreshChantsList() {
#ifdef BACKEND
	static constexpr std::pair<CHANT, epro::path_stringview> types[] = {
		/////kdiy///////
		{CHANT::SET,       EPRO_TEXT("set")},
		{CHANT::EQUIP,     EPRO_TEXT("equip")},
		{CHANT::DESTROY,   EPRO_TEXT("destroyed")},
		{CHANT::BANISH,    EPRO_TEXT("banished")},						
		{CHANT::DRAW,      EPRO_TEXT("draw")},	
		{CHANT::DAMAGE,    EPRO_TEXT("damage")},	
		{CHANT::RECOVER,   EPRO_TEXT("gainlp")},	
		{CHANT::NEXTTURN,  EPRO_TEXT("nextturn")},
		{CHANT::STARTUP,  EPRO_TEXT("startup")},
		{CHANT::BORED,  EPRO_TEXT("bored")},
		/////kdiy///////				
		{CHANT::SUMMON,    EPRO_TEXT("summon")},
		{CHANT::ATTACK,    EPRO_TEXT("attack")},
		{CHANT::ACTIVATE,  EPRO_TEXT("activate")}
	};
	/////kdiy//////
	for (auto list : ChantsList) {
		list.clear();
	}
	int i=-1;
	for(int i=0; i< 10; i++)
    {
		for (int j = 0; j < totcharacter; j++)
		{
			ChantSPList[i][j].clear();
		}
	}
	/////kdiy///////
	for (const auto& chantType : types) {
		std::vector<epro::path_string> searchPath;
		/////kdiy///////
		searchPath.push_back(fmt::format(EPRO_TEXT("./sound/{}"), chantType.second));
		searchPath.push_back(fmt::format(EPRO_TEXT("./sound/character/player/{}"), chantType.second));
		searchPath.push_back(fmt::format(EPRO_TEXT("./sound/character/atem/{}"), chantType.second));
		searchPath.push_back(fmt::format(EPRO_TEXT("./sound/character/kaiba/{}"), chantType.second));
		searchPath.push_back(fmt::format(EPRO_TEXT("./sound/character/joey/{}"), chantType.second));
		searchPath.push_back(fmt::format(EPRO_TEXT("./sound/character/donthousand/{}"), chantType.second));
		searchPath.push_back(fmt::format(EPRO_TEXT("./sound/character/shark/{}"), chantType.second));
		searchPath.push_back(fmt::format(EPRO_TEXT("./sound/character/yuma/{}"), chantType.second));
		for (auto path : searchPath) {
			Utils::MakeDirectory(path);
		}
		if(chantType.first != CHANT::SUMMON && chantType.first != CHANT::ATTACK && chantType.first != CHANT::ACTIVATE) {
			if(chantType.first == CHANT::SET) i=0;		
			if(chantType.first == CHANT::EQUIP) i=1;
			if(chantType.first == CHANT::DESTROY) i=2;
			if(chantType.first == CHANT::BANISH) i=3;
			if(chantType.first == CHANT::DRAW) i=4;
			if(chantType.first == CHANT::DAMAGE) i=5;
			if(chantType.first == CHANT::RECOVER) i=6;
			if(chantType.first == CHANT::NEXTTURN) i=7;
			if(chantType.first == CHANT::STARTUP) i=8;
			if(chantType.first == CHANT::BORED) i=9;
			if(i == -1) continue;
			for(int x=0; x< totcharacter; x++) {	
				for (auto& file : Utils::FindFiles(searchPath[x], mixer->GetSupportedSoundExtensions())) {
					auto conv = Utils::ToUTF8IfNeeded(searchPath[x] + EPRO_TEXT("/") + file);
					ChantSPList[i][x].push_back(conv);
				}
			}		
		} else {
		//for (auto& file : Utils::FindFiles(searchPath, mixer->GetSupportedSoundExtensions())) {
		for(int x=0; x< totcharacter; x++) {	
			for (auto& file : Utils::FindFiles(searchPath[x], mixer->GetSupportedSoundExtensions())) {
				//const auto filepath = fmt::format(EPRO_TEXT("{}/{}"), searchPath, file);
				const auto filepath = fmt::format(EPRO_TEXT("{}/{}"), searchPath[x], file);
				auto scode = Utils::GetFileName(file);
				try {
					uint32_t code = static_cast<uint32_t>(std::stoul(scode));
					auto key = std::make_pair(chantType.first, code);
					// if (code && !ChantsList.count(key))	
					// 	ChantsList[key] = fmt::format("{}/{}", working_dir, Utils::ToUTF8IfNeeded(fmt::format(EPRO_TEXT("{}/{}"), searchPath, file)));
					if (code && !ChantsList[x].count(key))
						ChantsList[x][key] = fmt::format("{}/{}", working_dir, Utils::ToUTF8IfNeeded(fmt::format(EPRO_TEXT("{}/{}"), searchPath[x], file)));
				}
				catch (...) {
					continue;
				}
			}
		}
		}
		/////kdiy///////
	}
#endif
}
void SoundManager::PlaySoundEffect(SFX sound) {
#ifdef BACKEND
	if(!soundsEnabled) return;
	if(sound >= SFX::SFX_TOTAL_SIZE) return;
	if(SFXList[sound].empty()) return;
	mixer->PlaySound(SFXList[sound]);
#endif
}
void SoundManager::PlayBGM(BGM scene, bool loop) {
#ifdef BACKEND
	auto& list = BGMList[scene];
	int count = list.size();
	if(musicEnabled && (scene != bgm_scene || !mixer->MusicPlaying()) && count > 0) {
		bgm_scene = scene;
		int bgm = (std::uniform_int_distribution<>(0, count - 1))(rnd);
		const std::string BGMName = fmt::format("{}/./sound/BGM/{}", working_dir, list[bgm]);
		/////kdiy/////
		std::string bgm_custom = "BGM/custom/";
		std::string bgm_menu = "BGM/menu/";
		std::string bgm_deck = "BGM/deck/";
		std::string bgm_win = "BGM/win/";
		std::string bgm_lose = "BGM/lose/";
		if(BGMName.find(bgm_menu) != std::string::npos && BGMName.find(bgm_deck) != std::string::npos && BGMName.find(bgm_win) != std::string::npos && BGMName.find(bgm_lose) != std::string::npos && bgm_now.find(bgm_custom) != std::string::npos) return;
		bgm_now = BGMName;
		/////kdiy/////
		mixer->PlayMusic(BGMName, loop);
	}
#endif
}
///////kdiy//////
void SoundManager::PlayCustomMusic(std::string num) {
#ifdef BACKEND
	if(soundsEnabled) {
		const std::string BGMName = fmt::format("{}/./sound/custom/{}.mp3", working_dir, num);
		if(Utils::FileExists(Utils::ToPathString(BGMName)))
		    mixer->PlaySound(BGMName);
	}
#endif
}
void SoundManager::PlayCustomBGM(std::string num) {
#ifdef BACKEND
	if (musicEnabled) {
		const std::string BGMName = fmt::format("{}/./sound/BGM/custom/{}.mp3", working_dir, num);
		if (Utils::FileExists(Utils::ToPathString(BGMName))) {
			if(mixer->MusicPlaying())
			    mixer->StopMusic();
			bgm_now = BGMName;
			mixer->PlayMusic(BGMName, gGameConfig->loopMusic);
		}
	}
#endif
}
//bool SoundManager::PlayChant(CHANT chant, uint32_t code) {
bool SoundManager::PlayChant(CHANT chant, uint32_t code, uint32_t code2, int player) {
///////kdiy//////
#ifdef BACKEND
	if(!soundsEnabled) return false;
	///////kdiy//////
	if(player < 0) return false;
	if(code == 0) {
		int i=-1;
		if(chant == CHANT::SET) i=0;		
		if(chant == CHANT::EQUIP) i=1;
		if(chant == CHANT::DESTROY) i=2;
		if(chant == CHANT::BANISH) i=3;
		if(chant == CHANT::DRAW) i=4;
		if(chant == CHANT::DAMAGE) i=5;
		if(chant == CHANT::RECOVER) i=6;
		if(chant == CHANT::NEXTTURN) i=7;
		if(chant == CHANT::STARTUP) i=8;
		if(chant == CHANT::BORED) i=9;
		if(i == -1) return false;
		std::vector<std::string> list;
		list = ChantSPList[i][character[player]];
		int count = list.size();
		if(count > 0) {
			int bgm = (std::uniform_int_distribution<>(0, count - 1))(rnd);
			std::string BGMName = list[bgm];
			mixer->PlaySound(BGMName);
			return true;
		}
	} else {
	auto key = std::make_pair(chant, code);
	auto key2 = std::make_pair(chant, code2);
	std::map<std::pair<CHANT, uint32_t>, std::string> clist;
	clist = ChantsList[character[player]];
	if (clist.count(key)) {
		mixer->PlaySound(clist[key]);
		return true;
	}
	else if (clist.count(key2)) {
		mixer->PlaySound(clist[key2]);
		return true;
	}
	}
	///////kdiy//////
#endif
	return false;
}
void SoundManager::SetSoundVolume(double volume) {
#ifdef BACKEND
	if(mixer)
		mixer->SetSoundVolume(volume);
#endif
}
void SoundManager::SetMusicVolume(double volume) {
#ifdef BACKEND
	if(mixer)
		mixer->SetMusicVolume(volume);
#endif
}
void SoundManager::EnableSounds(bool enable) {
#ifdef BACKEND
	if(mixer && !(soundsEnabled = enable))
		mixer->StopSounds();
#endif
}
void SoundManager::EnableMusic(bool enable) {
#ifdef BACKEND
	if(mixer && !(musicEnabled = enable))
		mixer->StopMusic();
#endif
}
void SoundManager::StopSounds() {
#ifdef BACKEND
	if(mixer)
		mixer->StopSounds();
#endif
}
void SoundManager::StopMusic() {
#ifdef BACKEND
	if(mixer)
		mixer->StopMusic();
#endif
}
void SoundManager::PauseMusic(bool pause) {
#ifdef BACKEND
	if(mixer)
		mixer->PauseMusic(pause);
#endif
}

void SoundManager::Tick() {
#ifdef BACKEND
	if(mixer)
		mixer->Tick();
#endif
}

} // namespace ygo
