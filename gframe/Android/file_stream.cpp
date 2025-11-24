#ifndef __ANDROID__
#error This file may only be compiled for android!
#endif

#include "../file_stream.h"
#include <cstdio>
#include <unistd.h>

#include "access_private.hpp"
#include "porting_android.h"

ACCESS_PRIVATE_FIELD(std::filebuf, FILE*, __file_);
ACCESS_PRIVATE_FIELD(std::filebuf, std::ios_base::openmode, __om_);

static constexpr const char* make_mdstring(std::ios_base::openmode mode) {
    using namespace std;
    switch (mode) {
    case ios_base::out:
    case ios_base::out | ios_base::trunc:
        return "w";
    case ios_base::out | ios_base::app:
    case ios_base::app:
        return "a";
    case ios_base::in:
        return "r";
    case ios_base::in | ios_base::out:
        return "r+";
    case ios_base::in | ios_base::out | ios_base::trunc:
        return "w+";
    case ios_base::in | ios_base::out | ios_base::app:
    case ios_base::in | ios_base::app:
        return "a+";
    case ios_base::out | ios_base::binary:
    case ios_base::out | ios_base::trunc | ios_base::binary:
        return "wb";
    case ios_base::out | ios_base::app | ios_base::binary:
    case ios_base::app | ios_base::binary:
        return "ab";
    case ios_base::in | ios_base::binary:
        return "rb";
    case ios_base::in | ios_base::out | ios_base::binary:
        return "r+b";
    case ios_base::in | ios_base::out | ios_base::trunc | ios_base::binary:
        return "w+b";
    case ios_base::in | ios_base::out | ios_base::app | ios_base::binary:
    case ios_base::in | ios_base::app | ios_base::binary:
        return "a+b";
    default:
        return nullptr;
    }
}

static constexpr const char* c_mode_to_java(epro::path_stringview mode) {
	if(mode.back() == 'b')
		mode.remove_suffix(1);
	if(mode == "w") {
		return "wt";
	}
	if(mode == "a") {
		return "wa";
	}
	if(mode == "r") {
		return "r";
	}
	if(mode == "r+") {
		return "rw";
	}
	if(mode == "w+") {
		return "rwt";
	}
	if(mode == "a+") {
		return "rwa";
	}
	unreachable();
}

FileStream::FileStream(std::string filename, FileMode mode) : std::fstream() {
	if(!porting::pathIsUri(filename)) {
		open(filename, mode.streammode);
		return;
	}
	FILE* file = fileopen(filename, make_mdstring(mode.streammode));
	if(file == nullptr) {
		setstate(ios_base::failbit);
		return;
	}
	// hacky things, manually set up the fstream base class members so that it's using
	// our file handle under the hood
    auto& rdbuf_ref = *rdbuf();
    access_private::__file_(rdbuf_ref) = file;
    access_private::__om_(rdbuf_ref) = mode.streammode;
	clear();
}

FILE* fileopen(epro::path_stringview filename, epro::path_stringview mode) {
	if(!porting::pathIsUri(filename)) {
		return fopen(filename.data(), mode.data());
	}
	int fd = porting::openFdFromUri(filename, c_mode_to_java(mode));
	if(fd == -1)
		return nullptr;
    auto* file = fdopen(fd, mode.data());
	if(file)
		return file;
	close(fd);
	return nullptr;
}
