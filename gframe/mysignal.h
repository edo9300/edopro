#ifndef SIGNAL_H
#define SIGNAL_H

#include "epro_mutex.h"
#include "epro_condition_variable.h"
#include <atomic>

class Signal {
public:
	Signal():_nowait(false){}
	~Signal() { SetNoWait(true); }
	void Set() {
		val.notify_all();
	}
	void Wait() {
		if(_nowait)
			return;
		std::unique_lock<epro::mutex> lk(mut);
		val.wait(lk);
	}
	void Wait(std::unique_lock<epro::mutex>& _Lck) {
		if(_nowait)
			return;
		val.wait(_Lck);
	}
	void SetNoWait(bool nowait) {
		if((_nowait = nowait) == true)
			Set();
	}
private:
	epro::mutex mut;
	epro::condition_variable val;
	bool _nowait;
};

#endif // SIGNAL_H
