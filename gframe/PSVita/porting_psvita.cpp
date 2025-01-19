#include "porting_psvita.h"
#include <stdlib.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sqlite.h>

// stubbed functions for dependent libs
extern "C" {
	int readlink() {
		return -1;
	}
	int symlink() {
		return -1;
	}
	int getppid() {
		return 0;
	}
	int getpgid() {
		return 0;
	}
	int getpwuid_r() {
		return -1;
	}
}

namespace porting {

std::vector<epro::Address> getLocalIP() {
	return {};
}

void setupSqlite3() {
	// to use the sqlite shipped with the ps vita os, it needs to be
	// set up by providing the allocation functions it should use
	sceSysmoduleLoadModule(SCE_SYSMODULE_SQLITE);
	SceSqliteMallocMethods mf = {
		(void* (*) (int)) malloc,
		(void* (*) (void*, int)) realloc,
		free
	};
	sceSqliteConfigMallocMethods(&mf);
}

void print(epro::stringview str){
	sceClibPrintf("%s", str.data());
}

}
