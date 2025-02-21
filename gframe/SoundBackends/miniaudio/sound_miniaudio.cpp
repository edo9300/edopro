#ifdef YGOPRO_USE_MINIAUDIO
#include "sound_miniaudio.h"

#include <utility>
#include "../../fmt.h"
#include "../../utils.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4505) // unreferenced local function has been removed
#endif

namespace {
#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_INTEGER_CONVERSION
#include "stb_vorbis.h"
}

#define MA_API static
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION

#include <miniaudio.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// define the previously forward declared classes as just directly inheriting off the c
// structs, thus allowing to not include miniaudio in the header
class MaEngine final : public ma_engine {};
class MaSound final : public ma_sound {};
class MaSoundGroup final : public ma_sound_group {};

SoundMiniaudioBase::SoundMiniaudioBase() : engine{ nullptr, &FreeEngine }, sounds_group{ nullptr, &FreeSoundGroup }, music{nullptr, &FreeSound}, sound_volume(0), music_volume(0) {
	{
		auto tmp_engine = std::make_unique<MaEngine>();
		if(auto res = ma_engine_init(nullptr, tmp_engine.get()); res != MA_SUCCESS) {
			throw std::runtime_error(epro::format("Failed to initialize miniaudio engine, {}", ma_result_description(res)));
		}
		engine = EnginePtr{ tmp_engine.release(), &FreeEngine };
	}
	{
		auto tmp_sound_group = std::make_unique<MaSoundGroup>();
		if(auto res = ma_sound_group_init(engine.get(), 0, nullptr, tmp_sound_group.get()); res != MA_SUCCESS) {
			throw std::runtime_error(epro::format("Failed to initialize sound group, {}", ma_result_description(res)));
		}
		sounds_group = SoundGroupPtr{ tmp_sound_group.release(), &FreeSoundGroup };
	}
}
SoundMiniaudioBase::~SoundMiniaudioBase() = default;

void SoundMiniaudioBase::SetSoundVolume(double volume) {
	sound_volume = static_cast<float>(volume);
	ma_sound_group_set_volume(sounds_group.get(), sound_volume);
}

void SoundMiniaudioBase::SetMusicVolume(double volume) {
	music_volume = static_cast<float>(volume);
	if(music)
		ma_sound_set_volume(music.get(), music_volume);
}

bool SoundMiniaudioBase::PlayMusic(const std::string& name, bool loop) {
	if(MusicPlaying() && cur_music == name)
		return true;

	auto snd = openSound(name, true);
	if(snd == nullptr)
		return false;

	ma_sound_set_volume(snd.get(), music_volume);

	ma_sound_set_looping(snd.get(), loop);

	if(ma_sound_start(snd.get()) != MA_SUCCESS)
		return false;

	cur_music = name;

	music = std::move(snd);
	return true;
}

template<typename Char, typename ...Args>
static auto sound_init_from_file(ma_engine* engine, Char* path, Args&&... args) {
	if constexpr(std::is_same_v<Char, wchar_t>) {
		return ma_sound_init_from_file_w(engine, path, std::forward<Args>(args)...);
	} else {
		return ma_sound_init_from_file(engine, path, std::forward<Args>(args)...);
	}
}

SoundMiniaudioBase::SoundPtr SoundMiniaudioBase::openSound(const std::string& name, bool isMusic) {
	auto snd = std::make_unique<MaSound>();
	if(sound_init_from_file(engine.get(), ygo::Utils::ToPathString(name).data(), MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH,
							isMusic ? nullptr : sounds_group.get(), nullptr, snd.get()) != MA_SUCCESS)
		return { nullptr, &FreeSound };
	return { snd.release(), &FreeSound };

}

bool SoundMiniaudioBase::PlaySound(const std::string& name) {
	auto snd = openSound(name, false);
	if(snd == nullptr)
		return false;

	if(ma_sound_start(snd.get()) != MA_SUCCESS)
		return false;

	sounds.emplace_back(std::move(snd));

	return true;
}

void SoundMiniaudioBase::StopSounds() {
	sounds.clear();
}

void SoundMiniaudioBase::StopMusic() {
	music.reset();
}

void SoundMiniaudioBase::PauseMusic(bool pause) {
	if(!MusicPlaying())
		return;
	if(pause) {
		ma_sound_stop(music.get());
	} else {
		ma_sound_start(music.get());
	}
}

bool SoundMiniaudioBase::MusicPlaying() {
	return music && !ma_sound_at_end(music.get());
}

void SoundMiniaudioBase::Tick() {
	for(auto it = sounds.begin(); it != sounds.end();) {
		if(ma_sound_at_end(it->get()))
			it = sounds.erase(it);
		else
			it++;
	}
}

void SoundMiniaudioBase::FreeEngine(MaEngine* engine) {
	if(!engine)
		return;
	ma_engine_uninit(engine);
	delete engine;
}

void SoundMiniaudioBase::FreeSound(MaSound* sound) {
	if(!sound)
		return;
	ma_sound_stop(sound);
	ma_sound_uninit(sound);
	delete sound;
}

void SoundMiniaudioBase::FreeSoundGroup(MaSoundGroup* sound_group) {
	if(!sound_group)
		return;
	ma_sound_group_stop(sound_group);
	ma_sound_group_uninit(sound_group);
	delete sound_group;
}
namespace {
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.h"
}

#endif //YGOPRO_USE_MINIAUDIO
