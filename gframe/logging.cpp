#include "logging.h"
#include <ctime>
#include "file_stream.h"

namespace ygo {

void ErrorLog(epro::stringview msg) {
	FileStream log{ EPRO_TEXT("error.log"), FileStream::out | FileStream::app };
	if (!log.good())
		return;
	auto now = std::time(nullptr);
	log << epro::format("[{:%Y-%m-%d %H:%M:%S}] {}", fmt::localtime(now), msg) << std::endl;
}

}
