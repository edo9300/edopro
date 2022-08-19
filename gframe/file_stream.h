#ifndef FILE_STREAM_H
#define FILE_STREAM_H

#if defined(__MINGW32__) && defined(UNICODE) && !(defined(_GLIBCXX_HAVE__WFOPEN) && defined(_GLIBCXX_USE_WCHAR_T))
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
	static constexpr FileMode in{ _O_RDONLY, std::ios::in };
	static constexpr FileMode binary{ _O_BINARY, static_cast<FileMode::mode_t>(0) };
	static constexpr FileMode out{ _O_WRONLY, std::ios::out };
	static constexpr FileMode trunc{ _O_TRUNC | _O_CREAT, static_cast<FileMode::mode_t>(0), _S_IWRITE };
};

constexpr inline FileMode operator|(const FileMode& flag1, const FileMode& flag2) {
	return { flag1.cmode | flag2.cmode, static_cast<FileMode::mode_t>(flag1.streammode | flag2.streammode), flag1.readperm | flag2.readperm };
}
#else
#include <fstream>

using FileStream = std::fstream;
#endif

#include <cstdio>
#ifdef UNICODE
#define fileopen(file, mode) _wfopen(file, L##mode)
#else
#define fileopen(file, mode) fopen(file, mode)
#endif

#endif