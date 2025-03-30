#ifndef SOUND_THREADED_BACKEND_H
#define SOUND_THREADED_BACKEND_H
#include <queue>
#include "../epro_thread.h"
#include "../epro_mutex.h"
#include "../epro_condition_variable.h"
#include "../sound_backend.h"

class SoundThreadedBackend : public SoundBackend {
public:
	~SoundThreadedBackend() override;
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
	std::vector<epro::path_stringview> GetSupportedSoundExtensions() const override {
		return m_BaseBackend->GetSupportedSoundExtensions();
	}
	std::vector<epro::path_stringview> GetSupportedMusicExtensions() const override {
		return m_BaseBackend->GetSupportedMusicExtensions();
	}
protected:
	explicit SoundThreadedBackend(std::unique_ptr<SoundBackend>&&);
private:
	enum class ActionType : uint8_t {
		SET_SOUND_VOLUME,
		SET_MUSIC_VOLUME,
		PLAY_MUSIC,
		PLAY_SOUND,
		STOP_SOUNDS,
		STOP_MUSIC,
		PAUSE_MUSIC,
		LOOP_MUSIC,
		MUSIC_PLAYING,
		TICK,
		TERMINATE
	};
	struct Response {
		bool answer;
		bool answered;
	};
	union Argument {
		struct {
			Response* response;
			const std::string* name;
			bool loop;
		} play_music;
		struct {
			Response* response;
			const std::string* name;
		} play_sound;
		struct {
			Response* response;
		} is_playing;
		double volume;
		bool pause;
		bool loop;
	};
	struct Action {
		ActionType type;
		Argument arg;
	};
	void BaseLoop();
	std::unique_ptr<SoundBackend> m_BaseBackend;
	epro::mutex m_ActionMutex;
	epro::condition_variable m_ActionCondVar;
	epro::mutex m_ResponseMutex;
	epro::condition_variable m_ResponseCondVar;
	std::queue<Action> m_Actions;
	epro::thread m_BaseThread;
	bool WaitForResponse(std::unique_lock<epro::mutex>& lock, Response& res) {
		m_ResponseCondVar.wait(lock, [&res] {return res.answered; });
		return res.answer;
	}
};

template<typename T>
class SoundThreadedBackendHelper final : public SoundThreadedBackend {
public:
	SoundThreadedBackendHelper() : SoundThreadedBackend(std::make_unique<T>()) {}
	virtual ~SoundThreadedBackendHelper() override = default;
};

#endif //SOUND_THREADED_BACKEND_H
