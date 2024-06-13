#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Tchar.h> //_tmain
#include <ShellAPI.h>
#include <memory>

extern "C" int _tmain(int argc, TCHAR * argv[]);

#ifdef UNICODE
#define CommandLineToArgv CommandLineToArgvW
#else
//Implementation taken from the WINE project
TCHAR** WINAPI CommandLineToArgv(TCHAR* cmdline, int* numargs) {
	int qcount, bcount;
	const TCHAR* s;
	TCHAR** argv;
	DWORD argc;
	TCHAR* d;

	if(!numargs) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return nullptr;
	}

	if(*cmdline == 0) {
		/* Return the path to the executable */
		DWORD len, deslen = MAX_PATH, size;

		size = sizeof(TCHAR*) * 2 + deslen * sizeof(TCHAR);
		for(;;) {
			if(!(argv = (TCHAR**)LocalAlloc(LMEM_FIXED, size))) return nullptr;
			len = GetModuleFileName(0, (TCHAR*)(argv + 2), deslen);
			if(!len) {
				LocalFree(argv);
				return nullptr;
			}
			if(len < deslen) break;
			deslen *= 2;
			size = sizeof(TCHAR*) * 2 + deslen * sizeof(TCHAR);
			LocalFree(argv);
		}
		argv[0] = (TCHAR*)(argv + 2);
		argv[1] = nullptr;
		*numargs = 1;

		return argv;
	}

	/* --- First count the arguments */
	argc = 1;
	s = cmdline;
	/* The first argument, the executable path, follows special rules */
	if(*s == '"') {
		/* The executable path ends at the next quote, no matter what */
		s++;
		while(*s)
			if(*s++ == '"')
				break;
	} else {
		/* The executable path ends at the next space, no matter what */
		while(*s && *s != ' ' && *s != '\t')
			s++;
	}
	/* skip to the first argument, if any */
	while(*s == ' ' || *s == '\t')
		s++;
	if(*s)
		argc++;

	/* Analyze the remaining arguments */
	qcount = bcount = 0;
	while(*s) {
		if((*s == ' ' || *s == '\t') && qcount == 0) {
			/* skip to the next argument and count it if any */
			while(*s == ' ' || *s == '\t')
				s++;
			if(*s)
				argc++;
			bcount = 0;
		} else if(*s == '\\') {
			/* '\', count them */
			bcount++;
			s++;
		} else if(*s == '"') {
			/* '"' */
			if((bcount & 1) == 0)
				qcount++; /* unescaped '"' */
			s++;
			bcount = 0;
			/* consecutive quotes, see comment in copying code below */
			while(*s == '"') {
				qcount++;
				s++;
			}
			qcount = qcount % 3;
			if(qcount == 2)
				qcount = 0;
		} else {
			/* a regular character */
			bcount = 0;
			s++;
		}
	}

	/* Allocate in a single lump, the string array, and the strings that go
	 * with it. This way the caller can make a single LocalFree() call to free
	 * both, as per MSDN.
	 */
	argv = (TCHAR**)LocalAlloc(LMEM_FIXED, (argc + 1) * sizeof(TCHAR*) + (lstrlen(cmdline) + 1) * sizeof(TCHAR));
	if(!argv)
		return nullptr;

	/* --- Then split and copy the arguments */
	argv[0] = d = lstrcpy((TCHAR*)(argv + argc + 1), cmdline);
	argc = 1;
	/* The first argument, the executable path, follows special rules */
	if(*d == '"') {
		/* The executable path ends at the next quote, no matter what */
		s = d + 1;
		while(*s) {
			if(*s == '"') {
				s++;
				break;
			}
			*d++ = *s++;
		}
	} else {
		/* The executable path ends at the next space, no matter what */
		while(*d && *d != ' ' && *d != '\t')
			d++;
		s = d;
		if(*s)
			s++;
	}
	/* close the executable path */
	*d++ = 0;
	/* skip to the first argument and initialize it if any */
	while(*s == ' ' || *s == '\t')
		s++;
	if(!*s) {
		/* There are no parameters so we are all done */
		argv[argc] = nullptr;
		*numargs = argc;
		return argv;
	}

	/* Split and copy the remaining arguments */
	argv[argc++] = d;
	qcount = bcount = 0;
	while(*s) {
		if((*s == ' ' || *s == '\t') && qcount == 0) {
			/* close the argument */
			*d++ = 0;
			bcount = 0;

			/* skip to the next one and initialize it if any */
			do {
				s++;
			} while(*s == ' ' || *s == '\t');
			if(*s)
				argv[argc++] = d;
		} else if(*s == '\\') {
			*d++ = *s++;
			bcount++;
		} else if(*s == '"') {
			if((bcount & 1) == 0) {
				/* Preceded by an even number of '\', this is half that
				 * number of '\', plus a quote which we erase.
				 */
				d -= bcount / 2;
				qcount++;
			} else {
				/* Preceded by an odd number of '\', this is half that
				 * number of '\' followed by a '"'
				 */
				d = d - bcount / 2 - 1;
				*d++ = '"';
			}
			s++;
			bcount = 0;
			/* Now count the number of consecutive quotes. Note that qcount
			 * already takes into account the opening quote if any, as well as
			 * the quote that lead us here.
			 */
			while(*s == '"') {
				if(++qcount == 3) {
					*d++ = '"';
					qcount = 0;
				}
				s++;
			}
			if(qcount == 2)
				qcount = 0;
		} else {
			/* a regular character */
			*d++ = *s++;
			bcount = 0;
		}
	}
	*d = '\0';
	argv[argc] = nullptr;
	*numargs = argc;

	return argv;
}
#endif

int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int) {
	if(AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
		FILE* fDummy;
		freopen_s(&fDummy, "CONIN$", "r", stdin);
		freopen_s(&fDummy, "CONOUT$", "w", stderr);
		freopen_s(&fDummy, "CONOUT$", "w", stdout);
	}
	int nArgs;
	auto szArglist = CommandLineToArgv(GetCommandLine(), &nArgs);
	return _tmain(nArgs, std::unique_ptr<TCHAR*, void(*)(TCHAR**)>(szArglist, [](TCHAR** ptr) {
		LocalFree((HLOCAL)ptr);
	}).get());
}
