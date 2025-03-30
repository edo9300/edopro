#include "ireadfile_sqlite.h"
#include <limits>
#include <cstdio>
#include <cstring>
#include <sqlite3.h>
#include <IReadFile.h>

 //===========================================================================

namespace {

#define IRR_VFS_NAME "irr-vfs"

constexpr int iVersion = 1;

struct irrfile_t {
	sqlite3_file base;
	long size;
	irr::io::IReadFile* file;
};

template <auto... ret, typename...Args>
auto vfsStub([[maybe_unused]] Args...) {
	return (ret, ...);
}

int fileRead(sqlite3_file* file, void* buffer, int len, sqlite3_int64 offset) {
	auto* irrfile = reinterpret_cast<irrfile_t*>(file);
	if(!irrfile->file || offset > LONG_MAX)
		return SQLITE_IOERR_SHORT_READ;
	if(!irrfile->file->seek((long)offset) || static_cast<int>(irrfile->file->read(buffer, len)) != len)
		return SQLITE_IOERR_SHORT_READ;
	return SQLITE_OK;
}

int fileFileSize(sqlite3_file* file, sqlite3_int64* size) {
	const auto* irrfile = reinterpret_cast<irrfile_t*>(file);
	*size = irrfile->file ? irrfile->size : 0;
	return SQLITE_OK;
}

int fileCheckReservedLock([[maybe_unused]] sqlite3_file* file, int* result) {
	*result = 0;
	return SQLITE_OK;
}

constexpr sqlite3_io_methods iomethods{
	iVersion,                 /* iVersion */
	vfsStub<SQLITE_OK>,       /* xClose */
	fileRead,                 /* xRead */
	vfsStub<SQLITE_READONLY>, /* xWrite */
	vfsStub<SQLITE_READONLY>, /* xTruncate */
	vfsStub<SQLITE_OK>,       /* xSync */
	fileFileSize,             /* xFileSize */
	vfsStub<SQLITE_OK>,       /* xLock */
	vfsStub<SQLITE_OK>,       /* xUnlock */
	fileCheckReservedLock,    /* xCheckReservedLock */
	vfsStub<SQLITE_OK>,       /* xFileControl */
	vfsStub<0>,               /* xSectorSize */
	vfsStub<0>                /* xDeviceCharacteristics */
};

//===========================================================================

int vfsOpen([[maybe_unused]] sqlite3_vfs* vfs, const char* path, sqlite3_file* file, int flags, int* outflags) {
	if(!(SQLITE_OPEN_READONLY & flags))
		return SQLITE_ERROR;

	void* ptr;
	if(std::sscanf(path, "%p", &ptr) != 1)
		return SQLITE_ERROR;

	auto* irrfile = reinterpret_cast<irrfile_t*>(file);
	irrfile->base = { &iomethods };
	irrfile->file = static_cast<irr::io::IReadFile*>(ptr);
	irrfile->size = irrfile->file->getSize();

	*outflags = SQLITE_OPEN_READONLY;
	return SQLITE_OK;
}

int vfsAccess([[maybe_unused]] sqlite3_vfs* vfs, [[maybe_unused]] const char* path, [[maybe_unused]] int flags, int* result) {
	*result = 0;
	return SQLITE_OK;
}

int vfsFullPathname([[maybe_unused]] sqlite3_vfs* vfs, const char* path, int len, char* fullpath) {
	sqlite3_snprintf(len, fullpath, "%s", path);
	return SQLITE_OK;
}

//===========================================================================

constexpr auto mxPathname = std::numeric_limits<uintptr_t>::digits / 2;

}

std::unique_ptr<sqlite3_vfs> irrsqlite_createfilesystem() {
	return std::unique_ptr<sqlite3_vfs>(new sqlite3_vfs
		{
			iVersion,                        /* iVersion */
			sizeof(irrfile_t),               /* szOsFile */
			mxPathname,                      /* mxPathname */
			nullptr,                         /* pNext */
			IRR_VFS_NAME,                    /* zName */
			nullptr,                         /* pAppData */
			vfsOpen,                         /* xOpen */
			vfsStub<SQLITE_OK>,              /* xDelete */
			vfsAccess,                       /* xAccess */
			vfsFullPathname,                 /* xFullPathname */
			vfsStub<(void*)nullptr>,         /* xDlOpen */
			vfsStub<>,                       /* xDlError */
			vfsStub<(void(*)(void))nullptr>, /* xDlSym */
			vfsStub<>,                       /* xDlClose */
			vfsStub<SQLITE_OK>,              /* xRandomness */
			vfsStub<SQLITE_OK>,              /* xSleep */
			vfsStub<SQLITE_OK>,              /* xCurrentTime */
			vfsStub<SQLITE_OK>               /* xGetLastError */
		}
	);
}

int irrdb_open(irr::io::IReadFile* reader, sqlite3 **ppDb, int flags) {
	char buff[mxPathname];
	if(std::snprintf(buff, sizeof(buff), "%p", static_cast<void*>(reader)) >= mxPathname)
		return SQLITE_ERROR;
	return sqlite3_open_v2(buff, ppDb, flags, IRR_VFS_NAME);
}

//===========================================================================
