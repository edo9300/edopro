#ifndef FILE_STREAM_H
#define FILE_STREAM_H

#include "compiler_features.h"

#if defined(__MINGW32__) && defined(UNICODE)
#include <fcntl.h>
#include <io.h>
#include <ext/stdio_filebuf.h>
#include <sys/stat.h>
#include "text_types.h"

struct FileMode {
	using mode_t = decltype(std::ios::in);
	int cmode;
	mode_t streammode;
	int readperm;
};
class Filebuf {
protected:
	int m_fd;
	Filebuf(epro::path_stringview file, const FileMode& mode) : m_fd(_wopen(file.data(), mode.cmode, mode.readperm)) {}
};
class FileStream : Filebuf, __gnu_cxx::stdio_filebuf<char>, public std::iostream {
public:
	FileStream(epro::path_stringview file, const FileMode& mode) : Filebuf(file, mode),
		__gnu_cxx::stdio_filebuf<char>(m_fd, mode.streammode), std::iostream(m_fd == -1 ? nullptr : this) {}
	static constexpr inline FileMode in{ _O_RDONLY, std::ios::in, _S_IREAD };
	static constexpr inline FileMode binary{ _O_BINARY, std::ios::binary };
	static constexpr inline FileMode out{ _O_WRONLY | _O_CREAT, std::ios::out, _S_IWRITE };
	static constexpr inline FileMode trunc{ _O_TRUNC, std::ios::trunc };
	static constexpr inline FileMode app{ _O_APPEND, std::ios::app };
};

constexpr inline FileMode operator|(const FileMode& flag1, const FileMode& flag2) {
	constexpr auto in_out = std::ios::in | std::ios::out;
	auto new_cmode = flag1.cmode | flag2.cmode;
	auto new_mode = static_cast<FileMode::mode_t>(flag1.streammode | flag2.streammode);
	if((new_mode & in_out) == in_out) {
		new_cmode &= ~(_O_WRONLY | _O_RDONLY);
		new_cmode |= _O_RDWR;
	}
	auto new_read_perms = flag1.readperm | flag2.readperm;
	return { new_cmode, new_mode, new_read_perms };
}
#elif EDOPRO_ANDROID
#include <fstream>

#include <fcntl.h>
#include <sys/stat.h>

struct FileMode {
	friend class FileStream;

	constexpr inline FileMode operator|(const FileMode& flag2) const {
		return { static_cast<FileMode::mode_t>(streammode | flag2.streammode) };
	}
private:
	using mode_t = decltype(std::ios::in);
	constexpr FileMode(mode_t mode) : streammode{ mode } {};
	mode_t streammode;
};

class FileStream : public std::fstream {
public:
	FileStream(std::string filename, FileMode mode);
	static constexpr inline FileMode in{ std::ios::in };
	static constexpr inline FileMode binary{ std::ios::binary };
	static constexpr inline FileMode out{ std::ios::out };
	static constexpr inline FileMode trunc{ std::ios::trunc };
	static constexpr inline FileMode app{ std::ios::app };
};

#else
#include <fstream>

using FileStream = std::fstream;
#endif

#include <cstdio>
#ifdef UNICODE
#define fileopen(file, mode) _wfopen(file, L##mode)
#else
#if EDOPRO_ANDROID
#include "text_types.h"
FILE* fileopen(epro::path_stringview file, epro::path_stringview mode);
#else
#define fileopen(file, mode) fopen(file, mode)
#endif //EDOPRO_ANDROID
#endif

#endif
