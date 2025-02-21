#ifndef SOUND_SFML_H
#define SOUND_SFML_H
#include "../sound_threaded_backend.h"
#include <vector>
#include <memory>
#include <map>
#include <string>
#include <sfAudio/Music.hpp>

namespace sf {
	class Sound;
	class SoundBuffer;
}

class SoundSFMLBase final : public SoundBackend {
public:
	SoundSFMLBase();
	~SoundSFMLBase() override;
	void SetSoundVolume(double volume) override;
	void SetMusicVolume(double volume) override;
	bool PlayMusic(const std::string& name, bool loop) override;
	bool PlaySound(const std::string& name) override;
	void StopSounds() override;
	void StopMusic() override;
	void PauseMusic(bool pause) override;
	bool MusicPlaying() override;
	void Tick() override;
private:
	std::string cur_music;
	sf::Music music;
	std::vector<std::unique_ptr<sf::Sound>> sounds;
	float music_volume, sound_volume;
	std::map<std::string, std::unique_ptr<sf::SoundBuffer>> buffers;
	const sf::SoundBuffer& LookupSound(const std::string& name);
};

using SoundSFML = SoundThreadedBackendHelper<SoundSFMLBase>;

#endif //SOUND_SFML_H
