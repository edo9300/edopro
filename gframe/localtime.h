#ifndef LOCALTIME_H
#define LOCALTIME_H

#include <ctime>

#include "compiler_features.h"

namespace epro {

inline std::tm localtime(std::time_t time) {
	std::tm out_time{};
#if EDOPRO_WINDOWS
	localtime_s(&out_time, &time);
#elif EDOPRO_LINUX || EDOPRO_APPLE
	localtime_r(&time, &out_time);
#else
	std::tm* tm = std::localtime(&time);
	if(tm) out_time = *tm;
#endif
	return out_time;
}

}

#endif // LOCALTIME_H
