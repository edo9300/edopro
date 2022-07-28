#ifndef FILE_STREAM_H
#define FILE_STREAM_H

#if defined(__MINGW32__) && defined(UNICODE)
#include <fcntl.h>
#include <io.h>
#include <ext/stdio_filebuf.h>
#include "text_types.h"

struct FileMode {
	int cmode;
	decltype(std::ios::in) streammode;
};
class Filebuf {
protected:
	int m_fd;
	Filebuf(epro::path_stringview file, const FileMode& mode) : m_fd(_wopen(file.data(), mode.cmode)) {}
};
class FileStream : Filebuf, __gnu_cxx::stdio_filebuf<char>, public std::iostream {
public:
	FileStream(epro::path_stringview file, const FileMode& mode) : Filebuf(file, mode),
		__gnu_cxx::stdio_filebuf<char>(m_fd, mode.streammode), std::iostream(m_fd == -1 ? nullptr : this) {}
};
constexpr FileMode in{ _O_RDONLY, std::ios::in };
constexpr FileMode binary_in{ _O_RDONLY | _O_BINARY, std::ios::in };
constexpr FileMode out{ _O_WRONLY, std::ios::out };
constexpr FileMode binary_out{ _O_WRONLY | _O_BINARY, std::ios::out };
constexpr FileMode binary_out_trunc{ _O_WRONLY | _O_TRUNC | _O_CREAT | _O_BINARY, std::ios::out };
#else
#include <fstream>

constexpr auto in = std::fstream::in;
constexpr auto binary_in = std::fstream::binary | std::fstream::in;
constexpr auto out = std::fstream::out;
constexpr auto binary_out = std::fstream::binary | std::fstream::out;
constexpr auto binary_out_trunc = std::fstream::binary | std::fstream::trunc | std::fstream::out;
using FileStream = std::fstream;
#endif

#include <cstdio>
#ifdef UNICODE
#define fileopen(file, mode) _wfopen(file, L##mode)
#else
#define fileopen(file, mode) fopen(file, mode)
#endif

#endif