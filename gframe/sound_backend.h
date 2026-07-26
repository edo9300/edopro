#ifndef SOUND_BACKEND_H
#define SOUND_BACKEND_H
#include <memory>
#include <string>
#include <vector>
#include "text_types.h"

class SoundBackend {
public:
	virtual ~SoundBackend() = default;
	virtual void SetSoundVolume(double volume) = 0;
	virtual void SetMusicVolume(double volume) = 0;
	virtual bool PlayMusic(const std::string& name, bool loop) = 0;
	virtual bool PlaySound(const std::string& name) = 0;
	virtual void StopSounds() = 0;
	virtual void StopMusic() = 0;
	virtual bool MusicPlaying() = 0;
	virtual void PauseMusic(bool pause) = 0;
	virtual void LoopMusic(bool loop) = 0;
	virtual void Tick() {};
	virtual std::vector<epro::path_stringview> GetSupportedSoundExtensions() const {
		return { EPRO_TEXT("wav"), EPRO_TEXT("mp3"), EPRO_TEXT("ogg"), EPRO_TEXT("flac") };
	};
	virtual std::vector<epro::path_stringview> GetSupportedMusicExtensions() const {
		return { EPRO_TEXT("mp3"), EPRO_TEXT("ogg"), EPRO_TEXT("wav"), EPRO_TEXT("flac") };
	};
};

template<typename T>
class SoundBackendHelper {
public:
	static std::unique_ptr<SoundBackend> make_ptr();
};

#endif //SOUND_BACKEND_H
