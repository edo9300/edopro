#define WIN32_LEAN_AND_MEAN
#include "sound_manager.h"
#include "utils.h"
#include "config.h"
#include "fmt.h"
#if defined(YGOPRO_USE_IRRKLANG)
#include "SoundBackends/irrklang/sound_irrklang.h"
#endif
#if defined(YGOPRO_USE_SDL_MIXER)
#include "SoundBackends/sdlmixer/sound_sdlmixer.h"
#endif
#if defined(YGOPRO_USE_SDL_MIXER3)
#include "SoundBackends/sdlmixer3/sound_sdlmixer3.h"
#endif
#define YGOPRO_USE_SFML
#if defined(YGOPRO_USE_SFML)
#include "SoundBackends/sfml/sound_sfml.h"
#endif
#if defined(YGOPRO_USE_MINIAUDIO)
#include "SoundBackends/miniaudio/sound_miniaudio.h"
#endif

namespace ygo {
namespace {
std::unique_ptr<SoundBackend> make_backend(SoundManager::BACKEND backend) {
	switch(backend) {
#ifdef YGOPRO_USE_IRRKLANG
		case SoundManager::IRRKLANG:
			return std::make_unique<SoundIrrklang>();
#endif
#ifdef YGOPRO_USE_SDL_MIXER
		case SoundManager::SDL:
			return std::make_unique<SoundMixer>();
#endif
#ifdef YGOPRO_USE_SDL_MIXER3
		case SoundManager::SDL3:
			return std::make_unique<SoundMixer3>();
#endif
#ifdef YGOPRO_USE_SFML
		case SoundManager::SFML:
			return std::make_unique<SoundSFML>();
#endif
#ifdef YGOPRO_USE_MINIAUDIO
		case SoundManager::MINIAUDIO:
			return std::make_unique<SoundMiniaudio>();
#endif
		default:
			epro::print("{} sound backend not compiled in.\n", backend);
			[[fallthrough]];
		case SoundManager::NONE:
			return nullptr;
	}
}
}

SoundManager::SoundManager(double sounds_volume, double music_volume, bool sounds_enabled, bool music_enabled, BACKEND backend) {
	if(backend == DEFAULT) {
		backend = GetDefaultBackend();
	}
	epro::print("Using: {} for audio playback.\n", backend);
	if(backend == NONE) {
		soundsEnabled = musicEnabled = false;
		return;
	}
	working_dir = Utils::ToUTF8IfNeeded(Utils::GetWorkingDirectory());
	soundsEnabled = sounds_enabled;
	musicEnabled = music_enabled;
	try {
		auto tmp_mixer = make_backend(backend);
		if(!tmp_mixer) {
			epro::print("Failed to initialize audio backend:\n");
			soundsEnabled = musicEnabled = false;
			return;
		}
		tmp_mixer->SetMusicVolume(music_volume);
		tmp_mixer->SetSoundVolume(sounds_volume);
		mixer = std::move(tmp_mixer);
	}
	catch(const std::runtime_error& e) {
		epro::print("Failed to initialize audio backend:\n");
		epro::print(e.what());
		soundsEnabled = musicEnabled = false;
		return;
	}
	catch(...) {
		epro::print("Failed to initialize audio backend.\n");
		soundsEnabled = musicEnabled = false;
		return;
	}
	rnd.seed(static_cast<uint32_t>(time(0)));
	bgm_scene = -1;
	RefreshBGMList();
	RefreshSoundsList();
	RefreshChantsList();
}
bool SoundManager::IsUsable() {
	return mixer != nullptr;
}
void SoundManager::RefreshBGMList() {
	if(!IsUsable())
		return;
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
}
void SoundManager::RefreshSoundsList() {
	if(!IsUsable())
		return;
	static constexpr std::pair<SFX, epro::path_stringview> fx[]{
		{SUMMON, EPRO_TEXT("./sound/summon.{}"sv)},
		{SPECIAL_SUMMON, EPRO_TEXT("./sound/specialsummon.{}"sv)},
		{ACTIVATE, EPRO_TEXT("./sound/activate.{}"sv)},
		{SET, EPRO_TEXT("./sound/set.{}"sv)},
		{FLIP, EPRO_TEXT("./sound/flip.{}"sv)},
		{REVEAL, EPRO_TEXT("./sound/reveal.{}"sv)},
		{EQUIP, EPRO_TEXT("./sound/equip.{}"sv)},
		{DESTROYED, EPRO_TEXT("./sound/destroyed.{}"sv)},
		{BANISHED, EPRO_TEXT("./sound/banished.{}"sv)},
		{TOKEN, EPRO_TEXT("./sound/token.{}"sv)},
		{ATTACK, EPRO_TEXT("./sound/attack.{}"sv)},
		{DIRECT_ATTACK, EPRO_TEXT("./sound/directattack.{}"sv)},
		{DRAW, EPRO_TEXT("./sound/draw.{}"sv)},
		{SHUFFLE, EPRO_TEXT("./sound/shuffle.{}"sv)},
		{DAMAGE, EPRO_TEXT("./sound/damage.{}"sv)},
		{RECOVER, EPRO_TEXT("./sound/gainlp.{}"sv)},
		{COUNTER_ADD, EPRO_TEXT("./sound/addcounter.{}"sv)},
		{COUNTER_REMOVE, EPRO_TEXT("./sound/removecounter.{}"sv)},
		{COIN, EPRO_TEXT("./sound/coinflip.{}"sv)},
		{DICE, EPRO_TEXT("./sound/diceroll.{}"sv)},
		{NEXT_TURN, EPRO_TEXT("./sound/nextturn.{}"sv)},
		{PHASE, EPRO_TEXT("./sound/phase.{}"sv)},
		{PLAYER_ENTER, EPRO_TEXT("./sound/playerenter.{}"sv)},
		{CHAT, EPRO_TEXT("./sound/chatmessage.{}"sv)}
	};
	const auto extensions = mixer->GetSupportedSoundExtensions();
	for(const auto& sound : fx) {
		for(const auto& ext : extensions) {
			const auto filename = epro::format(sound.second, ext);
			if(Utils::FileExists(filename)) {
				SFXList[sound.first] = Utils::ToUTF8IfNeeded(filename);
				break;
			}
		}
	}
}
void SoundManager::RefreshBGMDir(epro::path_stringview path, BGM scene) {
	if(!IsUsable())
		return;
	for(auto& file : Utils::FindFiles(epro::format(EPRO_TEXT("./sound/BGM/{}"), path), mixer->GetSupportedMusicExtensions())) {
		auto conv = Utils::ToUTF8IfNeeded(epro::format(EPRO_TEXT("{}/{}"), path, file));
		BGMList[BGM::ALL].push_back(conv);
		BGMList[scene].push_back(std::move(conv));
	}
}
void SoundManager::RefreshChantsList() {
	if(!IsUsable())
		return;
	static constexpr std::pair<CHANT, epro::path_stringview> types[]{
		{CHANT::SUMMON,    EPRO_TEXT("summon"sv)},
		{CHANT::ATTACK,    EPRO_TEXT("attack"sv)},
		{CHANT::ACTIVATE,  EPRO_TEXT("activate"sv)}
	};
	ChantsList.clear();
	for (const auto& chantType : types) {
		const epro::path_string searchPath = epro::format(EPRO_TEXT("./sound/{}"), chantType.second);
		Utils::MakeDirectory(searchPath);
		for (auto& file : Utils::FindFiles(searchPath, mixer->GetSupportedSoundExtensions())) {
			auto scode = Utils::GetFileName(file);
			try {
				uint32_t code = static_cast<uint32_t>(std::stoul(scode));
				auto key = std::make_pair(chantType.first, code);
				if (code && !ChantsList.count(key))
					ChantsList[key] = epro::format("{}/{}", working_dir, Utils::ToUTF8IfNeeded(epro::format(EPRO_TEXT("{}/{}"), searchPath, file)));
			}
			catch (...) {
				continue;
			}
		}
	}
}
void SoundManager::PlaySoundEffect(SFX sound) {
	if(!IsUsable())
		return;
	if(!soundsEnabled) return;
	if(sound >= SFX::SFX_TOTAL_SIZE) return;
	const auto& soundfile = SFXList[sound];
	if(soundfile.empty()) return;
	mixer->PlaySound(soundfile);
}
void SoundManager::PlayBGM(BGM scene, bool loop) {
	if(!IsUsable())
		return;
	if(!musicEnabled)
		return;
	auto& list = BGMList[scene];
	auto count = static_cast<int>(list.size());
	if(count == 0)
		return;
	if(scene != bgm_scene || !mixer->MusicPlaying()) {
		bgm_scene = scene;
		auto bgm = (std::uniform_int_distribution<>(0, count - 1))(rnd);
		const std::string BGMName = epro::format("{}/./sound/BGM/{}", working_dir, list[bgm]);
		if(!mixer->PlayMusic(BGMName, loop)) {
			// music failed to load, directly remove it from the list
			currentlyLooping = loop;
			list.erase(std::next(list.begin(), bgm));
		}
	} else if(loop != currentlyLooping) {
		currentlyLooping = loop;
		mixer->LoopMusic(loop);
	}
}
bool SoundManager::PlayChant(CHANT chant, uint32_t code) {
	if(!IsUsable())
		return false;
	if(!soundsEnabled) return false;
	auto key = std::make_pair(chant, code);
	auto chant_it = ChantsList.find(key);
	if(chant_it == ChantsList.end())
		return false;
	return mixer->PlaySound(chant_it->second);
}
void SoundManager::SetSoundVolume(double volume) {
	if(!IsUsable())
		return;
	mixer->SetSoundVolume(volume);
}
void SoundManager::SetMusicVolume(double volume) {
	if(!IsUsable())
		return;
	mixer->SetMusicVolume(volume);
}
void SoundManager::EnableSounds(bool enable) {
	if(!IsUsable())
		return;
	if(!(soundsEnabled = enable))
		mixer->StopSounds();
}
void SoundManager::EnableMusic(bool enable) {
	if(!IsUsable())
		return;
	if(!(musicEnabled = enable))
		mixer->StopMusic();
}
void SoundManager::StopSounds() {
	if(!IsUsable())
		return;
	mixer->StopSounds();
}
void SoundManager::StopMusic() {
	if(!IsUsable())
		return;
	mixer->StopMusic();
}
void SoundManager::PauseMusic(bool pause) {
	if(!IsUsable())
		return;
	mixer->PauseMusic(pause);
}

void SoundManager::Tick() {
	if(!IsUsable())
		return;
	mixer->Tick();
}

} // namespace ygo
