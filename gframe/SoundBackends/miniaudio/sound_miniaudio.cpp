#ifdef YGOPRO_USE_MINIAUDIO
#include "sound_miniaudio.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../../compiler_features.h"
#include "../../fmt.h"
#include "../../utils.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4505) // unreferenced local function has been removed
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-result"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace {
#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_INTEGER_CONVERSION
#include "stb_vorbis.h"
}

#define MA_API static
#define MA_NO_GENERATION
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#if EDOPRO_WINDOWS
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
// needed to support drag and drop
#define MA_COINIT_VALUE 0x2 /*COINIT_APARTMENTTHREADED*/
#endif

#define MA_ON_THREAD_ENTRY do {ygo::Utils::SetThreadName("miniaudio");} while(0);

#include "miniaudio.h"
#ifdef PlaySound
#undef PlaySound
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif

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
	void LoopMusic(bool loop) override;
	bool MusicPlaying() override;
	void Tick() override;
private:
	static void FreeEngine(ma_engine* engine);
	static void FreeSound(ma_sound* sound);
	static void FreeSoundGroup(ma_sound_group* sound_group);
	using EnginePtr = std::unique_ptr<ma_engine, decltype(&FreeEngine)>;
	using SoundPtr = std::unique_ptr<ma_sound, decltype(&FreeSound)>;
	using SoundGroupPtr = std::unique_ptr<ma_sound_group, decltype(&FreeSoundGroup)>;

	static SoundPtr AdoptSoundPointer(std::unique_ptr<ma_sound> soundPtr);

	ma_sound* getCachedSound(const std::string& name);
	SoundPtr openSound(const std::string& name);

	std::string cur_music;
	EnginePtr engine;
	SoundGroupPtr sounds_group;
	std::vector<SoundPtr> playing_sounds;
	std::map<std::string, SoundPtr> cached_sounds;
	SoundPtr music;
	float sound_volume, music_volume;
};

template<>
std::unique_ptr<SoundBackend> SoundBackendHelper<SoundMiniaudioBase>::make_ptr() {
	return std::make_unique<SoundMiniaudioBase>();
}

SoundMiniaudioBase::SoundMiniaudioBase() : engine{ nullptr, &FreeEngine }, sounds_group{ nullptr, &FreeSoundGroup }, music{nullptr, &FreeSound}, sound_volume(0), music_volume(0) {
	{
		auto tmp_engine = std::make_unique<ma_engine>();
		if(auto res = ma_engine_init(nullptr, tmp_engine.get()); res != MA_SUCCESS) {
			throw std::runtime_error(epro::format("Failed to initialize miniaudio engine, {}", ma_result_description(res)));
		}
		engine = EnginePtr{ tmp_engine.release(), &FreeEngine };
		ma_log_register_callback(ma_engine_get_log(engine.get()),
								 ma_log_callback_init(
									 []([[maybe_unused]] void* userdata, ma_uint32 level, const char* message) {
											epro::print("Miniaudio {}: {}\n", ma_log_level_to_string(level), message);
										}, nullptr));
	}
	{
		auto tmp_sound_group = std::make_unique<ma_sound_group>();
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

constexpr auto SOUND_INIT_FLAGS = MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;

template<typename Char, typename ...Args>
static auto sound_init_from_file(ma_engine* engine, Char* path, Args&&... args) {
	if constexpr(std::is_same_v<Char, wchar_t>) {
		return ma_sound_init_from_file_w(engine, path, std::forward<Args>(args)...);
	} else {
		return ma_sound_init_from_file(engine, path, std::forward<Args>(args)...);
	}
}

bool SoundMiniaudioBase::PlayMusic(const std::string& name, bool loop) {
	if(MusicPlaying() && cur_music == name)
		return true;

	auto snd = std::make_unique<ma_sound>();
	if(sound_init_from_file(engine.get(), ygo::Utils::ToPathString(name).data(),
							SOUND_INIT_FLAGS | MA_SOUND_FLAG_STREAM, nullptr, nullptr, snd.get()) != MA_SUCCESS)
		return false;

	ma_sound_set_volume(snd.get(), music_volume);

	ma_sound_set_looping(snd.get(), loop);

	if(ma_sound_start(snd.get()) != MA_SUCCESS)
		return false;

	cur_music = name;

	music = AdoptSoundPointer(std::move(snd));
	return true;
}

SoundMiniaudioBase::SoundPtr SoundMiniaudioBase::AdoptSoundPointer(std::unique_ptr<ma_sound> soundPtr) {
	return { soundPtr.release(), &FreeSound };
}

ma_sound* SoundMiniaudioBase::getCachedSound(const std::string& name) {
	auto it = cached_sounds.find(name);
	if(it != cached_sounds.end())
		return it->second.get();

	auto snd = std::make_unique<ma_sound>();
	if(sound_init_from_file(engine.get(), ygo::Utils::ToPathString(name).data(),
							SOUND_INIT_FLAGS, sounds_group.get(), nullptr, snd.get()) != MA_SUCCESS)
		return nullptr;

	return cached_sounds.emplace(name, AdoptSoundPointer(std::move(snd))).first->second.get();
}

SoundMiniaudioBase::SoundPtr SoundMiniaudioBase::openSound(const std::string& name) {
	auto* cache_snd = getCachedSound(name);
	if(cache_snd) {
		auto snd = std::make_unique<ma_sound>();
		if(ma_sound_init_copy(engine.get(), cache_snd, SOUND_INIT_FLAGS, sounds_group.get(), snd.get()) == MA_SUCCESS) {
			return AdoptSoundPointer(std::move(snd));
		}
	}
	return AdoptSoundPointer(nullptr);
}

bool SoundMiniaudioBase::PlaySound(const std::string& name) {
	auto snd = openSound(name);
	if(snd == nullptr)
		return false;

	if(ma_sound_start(snd.get()) != MA_SUCCESS)
		return false;

	playing_sounds.emplace_back(std::move(snd));

	return true;
}

void SoundMiniaudioBase::StopSounds() {
	playing_sounds.clear();
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

void SoundMiniaudioBase::LoopMusic(bool loop) {
	if(!MusicPlaying())
		return;
	if(!!ma_sound_is_looping(music.get()) != loop) {
		ma_sound_set_looping(music.get(), loop);
	}
}

bool SoundMiniaudioBase::MusicPlaying() {
	return music && !ma_sound_at_end(music.get());
}

void SoundMiniaudioBase::Tick() {
	for(auto it = playing_sounds.begin(); it != playing_sounds.end();) {
		if(ma_sound_at_end(it->get()))
			it = playing_sounds.erase(it);
		else
			it++;
	}
}

void SoundMiniaudioBase::FreeEngine(ma_engine* engine) {
	if(!engine)
		return;
	ma_engine_uninit(engine);
	delete engine;
}

void SoundMiniaudioBase::FreeSound(ma_sound* sound) {
	if(!sound)
		return;
	ma_sound_stop(sound);
	ma_sound_uninit(sound);
	delete sound;
}

void SoundMiniaudioBase::FreeSoundGroup(ma_sound_group* sound_group) {
	if(!sound_group)
		return;
	ma_sound_group_stop(sound_group);
	ma_sound_group_uninit(sound_group);
	delete sound_group;
}
namespace {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244) // conversion from 'int' to 'uintX', possible loss of data
#pragma warning(disable : 4456) // declaration of 'z' hides previous local declaration
#pragma warning(disable : 4457) // declaration of 'm' hides function parameter
#pragma warning(disable : 4245) // '=': conversion from 'int' to '`uint32', signed/unsigned mismatch
#pragma warning(disable : 4701) // potentially uninitialized local variable used
#endif

#ifdef __MINGW32__
#undef __STDC_WANT_SECURE_LIB__
#endif
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

#endif //YGOPRO_USE_MINIAUDIO
