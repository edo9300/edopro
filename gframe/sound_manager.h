#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <array>
#include <memory>
#include "RNG/mt19937.h"
#include <map>
#include "compiler_features.h"
#include "fmt.h"
#include "text_types.h"
#include "sound_backend.h"

namespace ygo {

class SoundManager {
public:
	enum BACKEND {
		DEFAULT,
		NONE,
		IRRKLANG,
		SDL,
		SDL3,
		SFML,
		MINIAUDIO,
	};
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
		CHAT,
		SFX_TOTAL_SIZE
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
	SoundManager(double sounds_volume, double music_volume, bool sounds_enabled, bool music_enabled, BACKEND backend);
	bool IsUsable();
	void RefreshBGMList();
	void RefreshChantsList();
	void PlaySoundEffect(SFX sound);
	void PlayBGM(BGM scene, bool loop = true);
	bool PlayChant(CHANT chant, uint32_t code);
	void SetSoundVolume(double volume);
	void SetMusicVolume(double volume);
	void EnableSounds(bool enable);
	void EnableMusic(bool enable);
	void StopSounds();
	void StopMusic();
	void PauseMusic(bool pause);
	void Tick();

	static constexpr auto GetSupportedBackends() {
		// NOTE: needed to support clang from android ndk 16b
		constexpr int array_elements = 2
#if defined(YGOPRO_USE_MINIAUDIO)
			+ 1
#endif
#if defined(YGOPRO_USE_SFML)
			+ 1
#endif
#if defined(YGOPRO_USE_SDL_MIXER3)
			+ 1
#endif
#if defined(YGOPRO_USE_SDL_MIXER)
			+ 1
#endif
#if defined(YGOPRO_USE_IRRKLANG)
			+ 1
#endif
		;
		return std::array<BACKEND, array_elements>{
			DEFAULT,
#if defined(YGOPRO_USE_MINIAUDIO)
			MINIAUDIO,
#endif
#if defined(YGOPRO_USE_SFML)
			SFML,
#endif
#if defined(YGOPRO_USE_SDL_MIXER3)
			SDL3,
#endif
#if defined(YGOPRO_USE_SDL_MIXER)
			SDL,
#endif
#if defined(YGOPRO_USE_IRRKLANG)
			IRRKLANG,
#endif
			NONE,
		};
	}

	static constexpr auto GetDefaultBackend() {
		return GetSupportedBackends()[1];
	}

	static constexpr bool HasMultipleBackends() {
		return GetSupportedBackends().size() > 3;
	}

	template<typename T = char>
	static constexpr auto GetBackendName(BACKEND backend) {
		switch(backend) {
			case IRRKLANG:
				return CHAR_T_STRINGVIEW(T, "Irrklang");
			case SDL:
				return CHAR_T_STRINGVIEW(T, "SDL");
			case SDL3:
				return CHAR_T_STRINGVIEW(T, "SDL3");
			case SFML:
				return CHAR_T_STRINGVIEW(T, "SFML");
			case MINIAUDIO:
				return CHAR_T_STRINGVIEW(T, "miniaudio");
			case NONE:
				return CHAR_T_STRINGVIEW(T, "none");
			case DEFAULT:
				return CHAR_T_STRINGVIEW(T, "default");
			default:
				unreachable();
		}
	}

private:
	std::vector<std::string> BGMList[8];
	std::string SFXList[SFX::SFX_TOTAL_SIZE];
	std::map<std::pair<CHANT, uint32_t>, std::string> ChantsList;
	int bgm_scene{ -1 };
	RNG::mt19937 rnd;
	std::unique_ptr<SoundBackend> mixer{ nullptr };
	void RefreshSoundsList();
	void RefreshBGMDir(epro::path_stringview path, BGM scene);
	bool soundsEnabled{ false };
	bool musicEnabled{ false };
	std::string working_dir{ "./" };
	bool currentlyLooping{ false };
};

extern SoundManager* gSoundManager;

}

template<typename CharT>
struct fmt::formatter<ygo::SoundManager::BACKEND, CharT> {
	template<typename ParseContext>
	constexpr auto parse(ParseContext& ctx) const { return ctx.begin(); }

	template <typename FormatContext>
	constexpr auto format(ygo::SoundManager::BACKEND value, FormatContext& ctx) const {
		return format_to(ctx.out(), CHAR_T_STRINGVIEW(CharT, "{}"), ygo::SoundManager::GetBackendName<CharT>(value));
	}
};

#endif //SOUNDMANAGER_H
