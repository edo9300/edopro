#include "sound_threaded_backend.h"
#include "utils.h"

SoundThreadedBackend::SoundThreadedBackend(std::unique_ptr<SoundBackend>&& base) :
	m_BaseBackend(std::move(base)),
	m_BaseThread(epro::thread(&SoundThreadedBackend::BaseLoop, this)) {
}

void SoundThreadedBackend::BaseLoop() {
	ygo::Utils::SetThreadName("SoundsThread");
	while(true) {
		std::unique_lock<epro::mutex> lck(m_ActionMutex);
		m_ActionCondVar.wait(lck, [this] { return !m_Actions.empty(); });
		auto action = std::move(m_Actions.front());
		m_Actions.pop();
		lck.unlock();
		switch(action.type) {
		case ActionType::SET_SOUND_VOLUME: {
			m_BaseBackend->SetSoundVolume(action.arg.volume);
			break;
		}
		case ActionType::SET_MUSIC_VOLUME: {
			m_BaseBackend->SetMusicVolume(action.arg.volume);
			break;
		}
		case ActionType::PLAY_MUSIC: {
			auto& argument = action.arg.play_music;
			auto& response = *argument.response;
			response.answer = m_BaseBackend->PlayMusic(*argument.name, argument.loop);
			std::lock_guard<epro::mutex> lckres(m_ResponseMutex);
			response.answered = true;
			m_ResponseCondVar.notify_all();
			break;
		}
		case ActionType::PLAY_SOUND: {
			auto& argument = action.arg.play_sound;
			auto& response = *argument.response;
			response.answer = m_BaseBackend->PlaySound(*argument.name);
			std::lock_guard<epro::mutex> lckres(m_ResponseMutex);
			response.answered = true;
			m_ResponseCondVar.notify_all();
			break;
		}
		case ActionType::STOP_SOUNDS: {
			m_BaseBackend->StopSounds();
			break;
		}
		case ActionType::STOP_MUSIC: {
			m_BaseBackend->StopMusic();
			break;
		}
		case ActionType::PAUSE_MUSIC: {
			m_BaseBackend->PauseMusic(action.arg.pause);
			break;
		}
		case ActionType::MUSIC_PLAYING: {
			auto& argument = action.arg.is_playing;
			auto& response = *argument.response;
			response.answer = m_BaseBackend->MusicPlaying();
			std::lock_guard<epro::mutex> lckres(m_ResponseMutex);
			response.answered = true;
			m_ResponseCondVar.notify_all();
			break;
		}
		case ActionType::TICK: {
			m_BaseBackend->Tick();
			break;
		}
		case ActionType::TERMINATE: {
			return;
		}
		}
	}
}

SoundThreadedBackend::~SoundThreadedBackend() {
	std::queue<Action> tmp;
	Action action{ ActionType::TERMINATE };
	tmp.push(action);
	{
		std::lock_guard<epro::mutex> lck(m_ActionMutex);
		m_Actions.swap(tmp);
		m_ActionCondVar.notify_all();
	}
	if(m_BaseThread.joinable())
		m_BaseThread.join();
}

void SoundThreadedBackend::SetSoundVolume(double volume) {
	Action action{ ActionType::SET_SOUND_VOLUME };
	action.arg.volume = volume;
	std::lock_guard<epro::mutex> lck(m_ActionMutex);
	m_Actions.push(action);
	m_ActionCondVar.notify_all();
}

void SoundThreadedBackend::SetMusicVolume(double volume) {
	Action action{ ActionType::SET_MUSIC_VOLUME };
	action.arg.volume = volume;
	std::lock_guard<epro::mutex> lck(m_ActionMutex);
	m_Actions.push(action);
	m_ActionCondVar.notify_all();
}

bool SoundThreadedBackend::PlayMusic(const std::string& name, bool loop) {
	Response res{};
	Action action{ ActionType::PLAY_MUSIC };
	auto& args = action.arg.play_music;
	args.response = &res;
	args.name = &name;
	args.loop = loop;
	std::unique_lock<epro::mutex> lck(m_ActionMutex);
	std::unique_lock<epro::mutex> lckres(m_ResponseMutex);
	m_Actions.push(action);
	m_ActionCondVar.notify_all();
	lck.unlock();
	return WaitForResponse(lckres, res);
}

bool SoundThreadedBackend::PlaySound(const std::string& name) {
	Response res{};
	Action action{ ActionType::PLAY_SOUND };
	action.arg.play_sound.name = &name;
	action.arg.play_sound.response = &res;
	std::unique_lock<epro::mutex> lck(m_ActionMutex);
	std::unique_lock<epro::mutex> lckres(m_ResponseMutex);
	m_Actions.push(action);
	m_ActionCondVar.notify_all();
	lck.unlock();
	return WaitForResponse(lckres, res);
}

void SoundThreadedBackend::StopSounds() {
	Action action{ ActionType::STOP_SOUNDS };
	std::lock_guard<epro::mutex> lck(m_ActionMutex);
	m_Actions.push(action);
	m_ActionCondVar.notify_all();
}

void SoundThreadedBackend::StopMusic() {
	Action action{ ActionType::STOP_MUSIC };
	std::lock_guard<epro::mutex> lck(m_ActionMutex);
	m_Actions.push(action);
	m_ActionCondVar.notify_all();
}

void SoundThreadedBackend::PauseMusic(bool pause) {
	Action action{ ActionType::PAUSE_MUSIC };
	action.arg.pause = pause;
	std::lock_guard<epro::mutex> lck(m_ActionMutex);
	m_Actions.push(action);
	m_ActionCondVar.notify_all();
}

bool SoundThreadedBackend::MusicPlaying() {
	Response res{};
	Action action{ ActionType::MUSIC_PLAYING };
	action.arg.is_playing.response = &res;
	std::unique_lock<epro::mutex> lck(m_ActionMutex);
	std::unique_lock<epro::mutex> lckres(m_ResponseMutex);
	m_Actions.push(action);
	m_ActionCondVar.notify_all();
	lck.unlock();
	return WaitForResponse(lckres, res);
}

void SoundThreadedBackend::Tick() {
	Action action{ ActionType::TICK };
	std::lock_guard<epro::mutex> lck(m_ActionMutex);
	m_Actions.push(action);
	m_ActionCondVar.notify_all();
}
