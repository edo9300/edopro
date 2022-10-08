#include "logging.h"
#include <ctime>
#include <fmt/chrono.h>
#include "file_stream.h"

namespace ygo {

void ErrorLog(epro::stringview msg) {
	FileStream log("error.log", FileStream::out | FileStream::app);
	if (!log.is_open())
		return;
	auto now = std::time(nullptr);
	log << epro::format("[{:%Y-%m-%d %H:%M:%S}] {}", *std::localtime(&now), msg) << std::endl;
}

}
