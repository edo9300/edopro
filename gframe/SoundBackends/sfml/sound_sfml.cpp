#ifdef YGOPRO_USE_SFML
#include "sound_sfml.h"
#include <vector>
#include <memory>
#include <map>
#include <string>

#include <sfAudio/Music.hpp>
#include <sfAudio/Sound.hpp>
#include <sfAudio/SoundBuffer.hpp>

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
	void LoopMusic(bool loop) override;
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

std::unique_ptr<SoundBackend> SoundSFML::make_ptr() {
	return std::make_unique<SoundSFMLBase>();
}

using Status = sf::SoundSource::Status;

SoundSFMLBase::SoundSFMLBase() :
	music(), music_volume(0.0f), sound_volume(0.0f) {
}
SoundSFMLBase::~SoundSFMLBase() = default;

void SoundSFMLBase::SetSoundVolume(double volume)
{
	sound_volume = (float)(volume * 100);
	for(auto& sound : sounds)
		sound->setVolume(sound_volume);
}

void SoundSFMLBase::SetMusicVolume(double volume)
{
	music_volume = (float)(volume * 100);
	music.setVolume(music_volume);
}

bool SoundSFMLBase::PlayMusic(const std::string& name, bool loop)
{
	if(MusicPlaying() && cur_music == name)
		return true;
	if(!music.openFromFile(name))
		return false;
	cur_music = name;
	music.setLoop(loop);
	music.play();
	return true;
}
const sf::SoundBuffer& SoundSFMLBase::LookupSound(const std::string& name)
{
	auto it = buffers.find(name);
	if (it == buffers.end()) {
		auto new_buf = std::make_unique<sf::SoundBuffer>();
		new_buf->loadFromFile(name);
		const auto ret = buffers.emplace(name, std::move(new_buf));
		it = ret.first;
	}
	return *it->second;
}

bool SoundSFMLBase::PlaySound(const std::string& name)
{
	auto& buf = LookupSound(name);
	if (buf.getSampleCount() == 0) return false;
	auto sound = std::make_unique<sf::Sound>(buf);
	if (!sound) return false;
	sound->setVolume(sound_volume);
	sound->play();
	sounds.emplace_back(std::move(sound));
	return true;
}

void SoundSFMLBase::StopSounds()
{
	sounds.clear();
	buffers.clear();
}

void SoundSFMLBase::StopMusic()
{
	music.stop();
}

void SoundSFMLBase::PauseMusic(bool pause)
{
	if(!MusicPlaying())
		return;
	if(pause)
		music.pause();
	else
		music.play();
}

void SoundSFMLBase::LoopMusic(bool loop)
{
	if(!MusicPlaying())
		return;
	if(music.getLoop() != loop)
		music.setLoop(loop);
}

bool SoundSFMLBase::MusicPlaying()
{
	return music.getStatus() != Status::Stopped;
}

void SoundSFMLBase::Tick()
{
	for (auto it = sounds.begin(); it != sounds.end();) {
		if ((*it)->getStatus() != Status::Playing)
			it = sounds.erase(it);
		else
			it++;
	}
}
#endif //YGOPRO_USE_SFML
