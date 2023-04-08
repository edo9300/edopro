#include "utils.h"
#include <cmath> // std::round
#include "config.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4091)
#endif
#include <dbghelp.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <shellapi.h> // ShellExecute
#include "utils_gui.h"
#else
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
using Stat = struct stat;
#include "porting.h"
#ifndef __ANDROID__
#include <sys/wait.h>
#endif //__ANDROID__
#if defined(__linux__)
#include <sys/sendfile.h>
#include <fcntl.h>
#elif defined(__APPLE__)
#ifdef EDOPRO_MACOS
#import <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <CoreServices/CoreServices.h>
#endif //EDOPRO_MACOS
#include <copyfile.h>
#endif //__linux__
#endif //_WIN32
#include <IFileArchive.h>
#include <IFileSystem.h>
#include <fmt/format.h>
#include <IOSOperator.h>
#include "bufferio.h"
#include "file_stream.h"
#ifdef USE_GLIBC_FILEBUF
constexpr FileMode FileStream::in;
constexpr FileMode FileStream::binary;
constexpr FileMode FileStream::out;
constexpr FileMode FileStream::trunc;
constexpr FileMode FileStream::app;
#endif

#if defined(_WIN32)
namespace {

#if defined(_MSC_VER)
//https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code?view=vs-2015&redirectedfrom=MSDN

static constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
#pragma pack(push, 8)
struct THREADNAME_INFO {
	DWORD dwType; // Must be 0x1000.
	LPCSTR szName; // Pointer to name (in user addr space).
	DWORD dwThreadID; // Thread ID (-1=caller thread).
	DWORD dwFlags; // Reserved for future use, must be zero.
};
#pragma pack(pop)
inline void NameThreadMsvc(const char* threadName) {
	const THREADNAME_INFO info{ 0x1000, threadName, ((DWORD)-1), 0 };
	__try {	RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info); }
	__except(EXCEPTION_EXECUTE_HANDLER) {}
}
#pragma warning(pop)
#endif
const auto PSetThreadDescription = [] {
	auto proc = GetProcAddress(GetModuleHandle(EPRO_TEXT("kernel32.dll")), "SetThreadDescription");
	if(proc == nullptr)
		proc = GetProcAddress(GetModuleHandle(EPRO_TEXT("KernelBase.dll")), "SetThreadDescription");
	using SetThreadDescription_t = HRESULT(WINAPI*)(HANDLE, PCWSTR);
	return reinterpret_cast<SetThreadDescription_t>(proc);
}();
void NameThread(const char* name, const wchar_t* wname) {
	(void)name;
	if(PSetThreadDescription)
		PSetThreadDescription(GetCurrentThread(), wname);
#if defined(_MSC_VER)
	NameThreadMsvc(name);
#endif //_MSC_VER
}

//Dump creation routines taken from Postgres
//https://github.com/postgres/postgres/blob/27b77ecf9f4d5be211900eda54d8155ada50d696/src/backend/port/win32/crashdump.c
LONG WINAPI crashDumpHandler(EXCEPTION_POINTERS* pExceptionInfo) {
	using MiniDumpWriteDump_t = BOOL(WINAPI*) (HANDLE hProcess, DWORD ProcessId, HANDLE hFile, MINIDUMP_TYPE DumpType,
											   PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
											   PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
											   PMINIDUMP_CALLBACK_INFORMATION CallbackParam
											   );
	ygo::GUIUtils::ShowErrorWindow("Crash", "The program crashed, a crash dump will be created");

	if(!ygo::Utils::MakeDirectory(EPRO_TEXT("./crashdumps")))
		return EXCEPTION_CONTINUE_SEARCH;

	/* 'crashdumps' exists and is a directory. Try to write a dump' */
	HANDLE selfProcHandle = GetCurrentProcess();
	DWORD selfPid = GetCurrentProcessId();

	MINIDUMP_EXCEPTION_INFORMATION ExInfo{ GetCurrentThreadId(), pExceptionInfo, FALSE };

	/* Load the dbghelp.dll library and functions */
	auto* dbgHelpDLL = LoadLibrary(EPRO_TEXT("dbghelp.dll"));
	if(dbgHelpDLL == nullptr)
		return EXCEPTION_CONTINUE_SEARCH;
	
	auto* miniDumpWriteDumpFn = reinterpret_cast<MiniDumpWriteDump_t>(GetProcAddress(dbgHelpDLL, "MiniDumpWriteDump"));

	if(miniDumpWriteDumpFn == nullptr) {
		FreeLibrary(dbgHelpDLL);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	/*
	* Dump as much as we can, except shared memory, code segments, and
	* memory mapped files. Exactly what we can dump depends on the
	* version of dbghelp.dll, see:
	* http://msdn.microsoft.com/en-us/library/ms680519(v=VS.85).aspx
	*/
	auto dumpType = static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithHandleData | MiniDumpWithDataSegs);

	if(GetProcAddress(dbgHelpDLL, "EnumDirTree") != nullptr) {
		/* If this function exists, we have version 5.2 or newer */
		dumpType = static_cast<MINIDUMP_TYPE>(dumpType | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithPrivateReadWriteMemory);
	}

	auto systemTicks = GetTickCount();
	const auto dumpPath = fmt::sprintf(EPRO_TEXT("./crashdumps/EDOPro-pid%0i-%0i.mdmp"), (int)selfPid, (int)systemTicks);
	
	auto dumpFile = CreateFile(dumpPath.data(), GENERIC_WRITE, FILE_SHARE_WRITE,
							   nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
							   nullptr);

	if(dumpFile == INVALID_HANDLE_VALUE) {
		FreeLibrary(dbgHelpDLL);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if(miniDumpWriteDumpFn(selfProcHandle, selfPid, dumpFile, dumpType, &ExInfo, nullptr, nullptr))
		ygo::GUIUtils::ShowErrorWindow("Crash dump", fmt::format("Succesfully wrote crash dump to file \"{}\"\n", ygo::Utils::ToUTF8IfNeeded(dumpPath)));

	CloseHandle(dumpFile);
	FreeLibrary(dbgHelpDLL);

	return EXCEPTION_CONTINUE_SEARCH;
}

}
#endif

namespace ygo {
	std::vector<SynchronizedIrrArchive> Utils::archives;
	irr::io::IFileSystem* Utils::filesystem{ nullptr };
	irr::IOSOperator* Utils::OSOperator{ nullptr };

	RNG::SplitMix64 Utils::generator(std::chrono::high_resolution_clock::now().time_since_epoch().count());

	void Utils::InternalSetThreadName(const char* name, const wchar_t* wname) {
		(void)wname;
#if defined(_WIN32)
		NameThread(name, wname);
#elif defined(__linux__)
		pthread_setname_np(pthread_self(), name);
#elif defined(__APPLE__)
		pthread_setname_np(name);
#endif //_WIN32
	}

	void Utils::SetupCrashDumpLogging() {
#ifdef _WIN32
		SetUnhandledExceptionFilter(crashDumpHandler);
#endif
	}

	thread_local std::string last_error_string;
	epro::stringview Utils::GetLastErrorString() {
		return last_error_string;
	}

	static void SetLastError() {
#ifdef _WIN32
		const auto error = GetLastError();
		if(error == NOERROR) {
			last_error_string.clear();
			return;
		}
		static constexpr DWORD formatControl = FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_IGNORE_INSERTS |
			FORMAT_MESSAGE_FROM_SYSTEM;

		LPTSTR textBuffer = nullptr;
		auto count = FormatMessage(formatControl, nullptr, error, 0, (LPTSTR)&textBuffer, 0, nullptr);
		if(count != 0) {
			last_error_string = Utils::ToUTF8IfNeeded(textBuffer);
			LocalFree(textBuffer);
			if(last_error_string.back() == '\n')
				last_error_string.pop_back();
			if(last_error_string.back() == '\r')
				last_error_string.pop_back();
			return;
		}
		last_error_string = "Unknown error";
#else
		last_error_string = strerror(errno);
#endif
	}

	static inline bool SetLastErrorStringIfFailed(bool check) {
		if(check)
			return true;
		SetLastError();
		return false;
	}

	bool Utils::MakeDirectory(epro::path_stringview path) {
#ifdef _WIN32
		return SetLastErrorStringIfFailed(CreateDirectory(path.data(), nullptr) || ERROR_ALREADY_EXISTS == GetLastError());
#else
		return SetLastErrorStringIfFailed(mkdir(path.data(), 0777) == 0 || errno == EEXIST);
#endif
	}
	bool Utils::FileCopyFD(int source, int destination) {
#if defined(__linux__)
		off_t bytesCopied = 0;
		Stat fileinfo{};
		fstat(source, &fileinfo);
		int result = sendfile(destination, source, &bytesCopied, fileinfo.st_size);
		return SetLastErrorStringIfFailed(result != -1);
#elif defined(__APPLE__)
		return SetLastErrorStringIfFailed(fcopyfile(source, destination, 0, COPYFILE_ALL) == 0);
#else
		(void)source;
		(void)destination;
		return false;
#endif
	}
	bool Utils::FileCopy(epro::path_stringview source, epro::path_stringview destination) {
		if(source == destination)
			return false;
#ifdef _WIN32
		return SetLastErrorStringIfFailed(CopyFile(source.data(), destination.data(), false));
#elif defined(__linux__)
		int input, output;
		if((input = open(source.data(), O_RDONLY)) == -1) {
			SetLastError();
			return false;
		}
		Stat fileinfo{};
		fstat(input, &fileinfo);
		if((output = creat(destination.data(), fileinfo.st_mode)) == -1) {
			SetLastError();
			close(input);
			return false;
		}
		auto result = FileCopyFD(input, output);
		close(input);
		close(output);
		return result;
#elif defined(__APPLE__)
		return SetLastErrorStringIfFailed(copyfile(source.data(), destination.data(), 0, COPYFILE_ALL) == 0);
#else
		FileStream src(source.data(), FileStream::in | FileStream::binary);
		if(!src.is_open())
			return false;
		FileStream dst(destination.data(), FileStream::out | FileStream::binary);
		if(!dst.is_open())
			return false;
		dst << src.rdbuf();
		src.close();
		dst.close();
		return true;
#endif
	}
	bool Utils::FileMove(epro::path_stringview source, epro::path_stringview destination) {
#ifdef _WIN32
		return SetLastErrorStringIfFailed(MoveFile(source.data(), destination.data()));
#else
		return SetLastErrorStringIfFailed(rename(source.data(), destination.data()) == 0);
#endif
	}
	bool Utils::FileExists(epro::path_stringview path) {
#ifdef _WIN32
		const auto dwAttrib = GetFileAttributes(path.data());
		return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
		Stat sb;
		return stat(path.data(), &sb) != -1 && S_ISREG(sb.st_mode) != 0;
#endif
	}
	static epro::path_string working_dir;
	bool Utils::SetWorkingDirectory(epro::path_stringview newpath) {
#ifdef _WIN32
		bool res = SetLastErrorStringIfFailed(SetCurrentDirectory(newpath.data()));
#elif defined(EDOPRO_IOS)
		bool res = porting::changeWorkDir(newpath.data()) == 1;
#else
		bool res = SetLastErrorStringIfFailed(chdir(newpath.data()) == 0);
#endif
		if(res)
			working_dir = NormalizePathImpl(newpath);
		return res;
	}
	const epro::path_string& Utils::GetWorkingDirectory() {
		return working_dir;
	}
	bool Utils::FileDelete(epro::path_stringview source) {
#ifdef _WIN32
		return SetLastErrorStringIfFailed(DeleteFile(source.data()));
#else
		return SetLastErrorStringIfFailed(remove(source.data()) == 0);
#endif
	}

	void Utils::FindFiles(epro::path_stringview _path, const std::function<void(epro::path_stringview, bool)>& cb) {
		const auto path = Utils::NormalizePath(_path);
#ifdef _WIN32
		WIN32_FIND_DATA fdata;
		auto fh = FindFirstFile(epro::format(EPRO_TEXT("{}*.*"), path).data(), &fdata);
		if(fh != INVALID_HANDLE_VALUE) {
			do {
				cb(fdata.cFileName, !!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
			} while(FindNextFile(fh, &fdata));
			FindClose(fh);
		}
#else
		if(auto dir = opendir(path.data())) {
#ifdef __ANDROID__
			// workaround, on android 11 (and probably higher) the folders "." and ".." aren't
			// returned by readdir, assuming the parsed path will never be root, manually
			// pass those 2 folders if they aren't returned by readdir
			bool found_curdir = false;
			bool found_topdir = false;
#endif //__ANDROID__
			while(auto dirp = readdir(dir)) {
				bool isdir = false;
#ifdef _DIRENT_HAVE_D_TYPE //avoid call to format and stat
				if(dirp->d_type != DT_LNK && dirp->d_type != DT_UNKNOWN) {
					isdir = dirp->d_type == DT_DIR;
					if(!isdir && dirp->d_type != DT_REG) //not a folder or file, skip
						continue;
				} else
#endif
				{
					Stat fileStat;
					stat(epro::format("{}{}", path, dirp->d_name).data(), &fileStat);
					isdir = !!S_ISDIR(fileStat.st_mode);
					if(!isdir && !S_ISREG(fileStat.st_mode)) //not a folder or file, skip
						continue;
				}
#ifdef __ANDROID__
				if(dirp->d_name == EPRO_TEXT("."_sv))
					found_curdir = true;
				if(dirp->d_name == EPRO_TEXT(".."_sv))
					found_topdir = true;
#endif //__ANDROID__
				cb(dirp->d_name, isdir);
			}
#ifdef __ANDROID__
			if(!found_curdir)
				cb(EPRO_TEXT("."), true);
			if(!found_topdir)
				cb(EPRO_TEXT(".."), true);
#endif //__ANDROID__
			closedir(dir);
		}
#endif
	}

#define ROOT_OR_CUR(str) (str == EPRO_TEXT(".") || (str == EPRO_TEXT("..")))
	bool Utils::ClearDirectory(epro::path_stringview path) {
		FindFiles(path, [&path](epro::path_stringview name, bool isdir) {
			if(isdir) {
				if(!ROOT_OR_CUR(name))
					DeleteDirectory(epro::format(EPRO_TEXT("{}{}/"), path, name));
			} else
				FileDelete(epro::format(EPRO_TEXT("{}{}"), path, name));
		});
		return true;
	}
	bool Utils::DeleteDirectory(epro::path_stringview source) {
		ClearDirectory(source);
#ifdef _WIN32
		return RemoveDirectory(source.data());
#else
		return rmdir(source.data()) == 0;
#endif
	}

	void Utils::CreateResourceFolders() {
		//create directories if missing
		MakeDirectory(EPRO_TEXT("deck"));
		MakeDirectory(EPRO_TEXT("puzzles"));
		MakeDirectory(EPRO_TEXT("pics"));
		MakeDirectory(EPRO_TEXT("pics/field"));
		MakeDirectory(EPRO_TEXT("pics/cover"));
		MakeDirectory(EPRO_TEXT("pics/temp/"));
		ClearDirectory(EPRO_TEXT("pics/temp/"));
		MakeDirectory(EPRO_TEXT("replay"));
		MakeDirectory(EPRO_TEXT("screenshots"));
	}

	std::vector<epro::path_string> Utils::FindFiles(epro::path_stringview path, const std::vector<epro::path_stringview>& extensions, int subdirectorylayers) {
		std::vector<epro::path_string> res;
		FindFiles(path, [&res, extensions, path, subdirectorylayers](epro::path_stringview name, bool isdir) {
			if(isdir) {
				if(subdirectorylayers && !ROOT_OR_CUR(name)) {
					auto res2 = FindFiles(epro::format(EPRO_TEXT("{}{}/"), path, name), extensions, subdirectorylayers - 1);
					for(auto& file : res2)
						file = epro::format(EPRO_TEXT("{}/{}"), name, file);
					res.insert(res.end(), std::make_move_iterator(res2.begin()), std::make_move_iterator(res2.end()));
				}
			} else {
				if(extensions.empty() || std::find(extensions.begin(), extensions.end(), Utils::GetFileExtension(name)) != extensions.end())
					res.emplace_back(name.data(), name.size());
			}
		});
		std::sort(res.begin(), res.end(), CompareIgnoreCase<epro::path_string>);
		return res;
	}
	std::vector<epro::path_string> Utils::FindSubfolders(epro::path_stringview path, int subdirectorylayers, bool addparentpath) {
		std::vector<epro::path_string> results;
		FindFiles(path, [&results, path, subdirectorylayers, addparentpath](epro::path_stringview name, bool isdir) {
			if (!isdir || ROOT_OR_CUR(name))
				return;
			epro::path_string fullpath = epro::format(EPRO_TEXT("{}{}/"), path, name);
			epro::path_stringview cur = name;
			if(addparentpath)
				cur = fullpath;
			results.emplace_back(cur.data(), cur.size());
			if(subdirectorylayers > 1) {
				auto subresults = FindSubfolders(fullpath, subdirectorylayers - 1, false);
				for(auto& folder : subresults) {
					folder = epro::format(EPRO_TEXT("{}{}/"), fullpath, folder);
				}
				results.insert(results.end(), std::make_move_iterator(subresults.begin()), std::make_move_iterator(subresults.end()));
			}
		});
		std::sort(results.begin(), results.end(), CompareIgnoreCase<epro::path_string>);
		return results;
	}
	std::vector<uint32_t> Utils::FindFiles(irr::io::IFileArchive* archive, epro::path_stringview path, const std::vector<epro::path_stringview>& extensions, int subdirectorylayers) {
		std::vector<uint32_t> res;
		auto list = archive->getFileList();
		for(irr::u32 i = 0; i < list->getFileCount(); i++) {
			if(list->isDirectory(i))
				continue;
			const auto name = list->getFullFileName(i);
			if(std::count(name.data(), name.data() + name.size(), EPRO_TEXT('/')) > subdirectorylayers)
				continue;
			if(extensions.empty() || std::find(extensions.begin(), extensions.end(), Utils::GetFileExtension(name)) != extensions.end())
				res.push_back(i);
		}
		return res;
	}
	irr::io::IReadFile* Utils::FindFileInArchives(epro::path_stringview path, epro::path_stringview name) {
		for(auto& archive : archives) {
			auto list = archive.archive->getFileList();
			int res = list->findFile(epro::format(EPRO_TEXT("{}{}"), path, name).data());
			if(res != -1) {
				std::lock_guard<epro::mutex> lk(*archive.mutex);
				auto reader = archive.archive->createAndOpenFile(res);
				return reader;
			}
		}
		return nullptr;
	}
	const std::string& Utils::GetUserAgent() {
		auto EscapeUTF8 = [](auto& to_escape) {
			auto IsNonANSI = [](char c) {
				return (static_cast<unsigned>(c) & ~0x7Fu) != 0;
			};
			const epro::stringview view{ to_escape.data(), to_escape.size() };
			const auto total_unicode = std::count_if(view.begin(), view.end(), IsNonANSI);
			std::string ret;
			ret.reserve(view.size() + (total_unicode * 4));
			for(auto c : view) {
				if(IsNonANSI(c)) {
					static constexpr std::array<char, 16> map{ {'0', '1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'} };
					auto int_char = static_cast<unsigned>(c);
					ret.append(R"(\x)").append(1, map[(int_char >> 4) & 0xf]).append(1, map[int_char & 0xf]);
				} else
					ret.append(1, c);
			}
			return ret;
		};
		static const std::string agent = epro::format("EDOPro-" OSSTRING "-" STR(EDOPRO_VERSION_MAJOR) "." STR(EDOPRO_VERSION_MINOR) "." STR(EDOPRO_VERSION_PATCH)" {}",
													  EscapeUTF8(Utils::OSOperator->getOperatingSystemVersion()));
		return agent;
	}
	epro::path_string Utils::GetAbsolutePath(epro::path_stringview path) {
#ifdef _WIN32
		epro::path_char fpath[MAX_PATH];
		auto len = GetFullPathName(path.data(), MAX_PATH, fpath, nullptr);
		epro::path_string ret{ fpath, len };
		std::replace(ret.begin(), ret.end(), EPRO_TEXT('\\'), EPRO_TEXT('/'));
		return ret;
#else
		epro::path_char* p = realpath(path.data(), nullptr);
		if(!p)
			return { path.data(), path.size() };
		epro::path_string ret{ p };
		free(p);
		return ret;
#endif
	}
	bool Utils::ContainsSubstring(epro::wstringview input, const std::vector<std::wstring>& tokens) {
		if (input.empty() || tokens.empty())
			return false;
		std::size_t pos1, pos2 = 0;
		for (const auto& token : tokens) {
			if((pos1 = input.find(token, pos2)) == epro::wstringview::npos)
				return false;
			pos2 = pos1 + token.size();
		}
		return true;
	}
	bool Utils::CreatePath(epro::path_stringview path, epro::path_string workingdir) {
		const bool wasempty = workingdir.empty();
		epro::path_stringview::size_type pos1, pos2 = 0;
		while((pos1 = path.find(EPRO_TEXT('/'), pos2)) != epro::path_stringview::npos) {
			if(pos1 != pos2) {
				if(pos2 != 0 || !wasempty)
					workingdir.append(1, EPRO_TEXT('/'));
				workingdir.append(path.begin() + pos2, path.begin() + pos1);
				if(!MakeDirectory(workingdir))
					return false;
			}
			pos2 = pos1 + 1;
		}
		return true;
	}

	static const epro::path_string exe_path = []()->epro::path_string {
#ifdef _WIN32
		TCHAR exepath[MAX_PATH];
		GetModuleFileName(nullptr, exepath, MAX_PATH);
		return Utils::NormalizePath<TCHAR>(exepath, false);
#elif defined(__linux__) && !defined(__ANDROID__)
		epro::path_char buff[PATH_MAX];
		ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
		if(len != -1)
			buff[len] = EPRO_TEXT('\0');
		return buff;
#elif defined(EDOPRO_MACOS)
		CFURLRef bundle_url = CFBundleCopyBundleURL(CFBundleGetMainBundle());
		CFStringRef bundle_path = CFURLCopyFileSystemPath(bundle_url, kCFURLPOSIXPathStyle);
		CFURLRef bundle_base_url = CFURLCreateCopyDeletingLastPathComponent(nullptr, bundle_url);
		CFRelease(bundle_url);
		CFStringRef path = CFURLCopyFileSystemPath(bundle_base_url, kCFURLPOSIXPathStyle);
		CFRelease(bundle_base_url);
		/*
		#ifdef MAC_OS_DISCORD_LAUNCHER
			//launches discord launcher so that it's registered as bundle to launch by discord
			system(epro::format("open {}/Contents/MacOS/discord-launcher.app --args random", CFStringGetCStringPtr(bundle_path, kCFStringEncodingUTF8)).data());
		#endif
		*/
		epro::path_string res = epro::path_string(CFStringGetCStringPtr(path, kCFStringEncodingUTF8)) + "/";
		CFRelease(path);
		CFRelease(bundle_path);
		return res;
#else
		return EPRO_TEXT("");
#endif
	}();
	const epro::path_string& Utils::GetExePath() {
		return exe_path;
	}

	static const epro::path_string exe_folder = Utils::GetFilePath(exe_path);
	const epro::path_string& Utils::GetExeFolder() {
		return exe_folder;
	}

	static const epro::path_string core_path = [] {
#ifdef _WIN32
		return epro::format(EPRO_TEXT("{}/ocgcore.dll"), Utils::GetExeFolder());
#else
		return EPRO_TEXT(""); // Unused on POSIX
#endif
	}();
	const epro::path_string& Utils::GetCorePath() {
		return core_path;
	}

	bool Utils::UnzipArchive(epro::path_stringview input, unzip_callback callback, unzip_payload* payload, epro::path_stringview dest) {
		thread_local char buff[0x2000];
		constexpr int buff_size = sizeof(buff) / sizeof(*buff);
		if(!filesystem)
			return false;
		CreatePath(dest, EPRO_TEXT("./"));
		irr::io::IFileArchive* archive = nullptr;
		if(!filesystem->addFileArchive({ input.data(), static_cast<irr::u32>(input.size()) }, false, false, irr::io::EFAT_ZIP, "", &archive))
			return false;

		archive->grab();
		auto filelist = archive->getFileList();
		auto count = filelist->getFileCount();

		irr::u32 totsize = 0;
		irr::u32 cur_fullsize = 0;

		for(irr::u32 i = 0; i < count; i++)
			totsize += filelist->getFileSize(i);

		if(payload)
			payload->tot = count;

		for(irr::u32 i = 0; i < count; i++) {
			auto filename = filelist->getFullFileName(i);
			bool isdir = filelist->isDirectory(i);
			if(isdir)
				CreatePath(epro::format(EPRO_TEXT("{}/"), filename), { dest.data(), dest.size() });
			else
				CreatePath({ filename.data(), filename.size() }, { dest.data(), dest.size() });
			if(!isdir) {
				int percentage = 0;
				auto reader = archive->createAndOpenFile(i);
				if(reader) {
					if(payload) {
						payload->is_new = true;
						payload->cur = i;
						payload->percentage = 0;
						payload->filename = filename.data();
						callback(payload);
						payload->is_new = false;
					}
					FileStream out{ epro::format(EPRO_TEXT("{}/{}"), dest, filename), FileStream::out | FileStream::binary };
					size_t r, rx = reader->getSize();
					for(r = 0; r < rx; /**/) {
						int wx = static_cast<int>(reader->read(buff, buff_size));
						out.write(buff, wx);
						r += wx;
						cur_fullsize += wx;
						if(callback && totsize) {
							double fractiondownloaded = (double)cur_fullsize / (double)rx;
							percentage = (int)std::round(fractiondownloaded * 100);
							if(payload)
								payload->percentage = percentage;
							callback(payload);
						}
					}
					reader->drop();
				}
			}
		}
		filesystem->removeFileArchive(archive);
		archive->drop();
		return true;
	}

	void Utils::SystemOpen(epro::path_stringview arg, OpenType type) {
#ifdef _WIN32
		if(type == SHARE_FILE)
			return;
		ShellExecute(nullptr, EPRO_TEXT("open"), (type == OPEN_FILE) ? epro::format(EPRO_TEXT("{}/{}"), GetWorkingDirectory(), arg).data() : arg.data(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__ANDROID__)
		switch(type) {
		case OPEN_FILE:
			return porting::openFile(epro::format("{}/{}", GetWorkingDirectory(), arg));
		case OPEN_URL:
			return porting::openUrl(arg);
		case SHARE_FILE:
			return porting::shareFile(epro::format("{}/{}", GetWorkingDirectory(), arg));
		}
#elif defined(EDOPRO_MACOS) || defined(__linux__)
		if(type == SHARE_FILE)
			return;
#ifdef EDOPRO_MACOS
#define OPEN "open"
#else
#define OPEN "xdg-open"
#endif
		const auto* arg_cstr = arg.data();
		auto pid = vfork();
		if(pid == 0) {
			execl("/usr/bin/" OPEN, OPEN, arg_cstr, nullptr);
			_exit(EXIT_FAILURE);
		} else if(pid < 0)
			perror("Failed to fork:");
		if(waitpid(pid, nullptr, WNOHANG) != 0)
			perror("Failed to open arg or file:");
#endif
	}

	void Utils::Reboot() {
#if !defined(__ANDROID__)
		const auto& path = GetExePath();
#ifdef _WIN32
		STARTUPINFO si{ sizeof(si) };
		PROCESS_INFORMATION pi{};
		auto command = epro::format(EPRO_TEXT("{} -C \"{}\" -l"), GetFileName(path, true), GetWorkingDirectory());
		if(!CreateProcess(path.data(), &command[0], nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi))
			return;
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
#else
#ifdef __linux__
		struct stat fileStat;
		stat(path.data(), &fileStat);
		chmod(path.data(), fileStat.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
#endif
		{
			const auto* path_cstr = path.data();
			const auto& workdir = GetWorkingDirectory();
			const auto* workdir_cstr = workdir.data();
			auto pid = vfork();
			if(pid == 0) {
#ifdef __linux__
				execl(path_cstr, path_cstr, "-C", workdir_cstr, "-l", nullptr);
#else
				(void)path_cstr;
				execlp("open", "open", "-b", "io.github.edo9300.ygoprodll", "--args", "-C", workdir_cstr, "-l", nullptr);
#endif
				_exit(EXIT_FAILURE);
			}
			if(pid < 0 || waitpid(pid, nullptr, WNOHANG) != 0)
				return;
		}
#endif
		exit(0);
#endif
	}

	std::wstring Utils::ReadPuzzleMessage(epro::wstringview script_name) {
		FileStream infile{ Utils::ToPathString(script_name), FileStream::in };
		if(infile.fail())
			return {};
		std::string str;
		std::string res = "";
		size_t start = std::string::npos;
		bool stop = false;
		while(!stop && std::getline(infile, str)) {
			auto pos = str.find('\r');
			if(str.size() && pos != std::string::npos)
				str.erase(pos);
			bool was_empty = str.empty();
			if(start == std::string::npos) {
				start = str.find("--[[message");
				if(start == std::string::npos)
					continue;
				str.erase(0, start + 11);
			}
			size_t end = str.find("]]");
			if(end != std::string::npos) {
				str.erase(end);
				stop = true;
			}
			if(str.empty() && !was_empty)
				continue;
			if(!res.empty() || was_empty)
				res += "\n";
			res += str;
		}
		return BufferIO::DecodeUTF8(res);
	}
}
