#include "utils.h"
#include <cmath> // std::round
#include "epro_thread.h"
#include "config.h"
#include "fmt.h"
#include "logging.h"
#include "deck_manager.h"
#include "replay.h"

#if EDOPRO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h> // ShellExecute
#include "utils_gui.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4091)
#endif
#include <dbghelp.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif //EDOPRO_WINDOWS

#if EDOPRO_LINUX_KERNEL || EDOPRO_APPLE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h> // PATH_MAX
using Stat = struct stat;
#include "porting.h"
#endif //EDOPRO_LINUX_KERNEL || EDOPRO_APPLE

#if EDOPRO_LINUX
#include <sys/wait.h>
#endif //EDOPRO_LINUX

#if EDOPRO_LINUX_KERNEL
#include <sys/sendfile.h>
#include <fcntl.h>
#endif //EDOPRO_LINUX_KERNEL

#if EDOPRO_APPLE
#if EDOPRO_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <CoreServices/CoreServices.h>
#endif //EDOPRO_MACOS
#include <copyfile.h>
#endif //EDOPRO_APPLE

#include <IFileArchive.h>
#include <IFileSystem.h>
#include <IOSOperator.h>
#include "bufferio.h"
#include "file_stream.h"

#if EDOPRO_WINDOWS
namespace {

//https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code?view=vs-2015&redirectedfrom=MSDN

struct THREADNAME_INFO {
	DWORD dwType; // Must be 0x1000.
	LPCSTR szName; // Pointer to name (in user addr space).
	DWORD dwThreadID; // Thread ID (-1=caller thread).
	DWORD dwFlags; // Reserved for future use, must be zero.
};

constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;

LONG NTAPI PvectoredExceptionHandler(EXCEPTION_POINTERS* ExceptionInfo) {
	if(ExceptionInfo->ExceptionRecord->ExceptionCode == MS_VC_EXCEPTION)
		return EXCEPTION_CONTINUE_EXECUTION;
	return EXCEPTION_CONTINUE_SEARCH;
}

inline void NameThreadMsvc(const char* threadName) {
	const THREADNAME_INFO info{ 0x1000, threadName, static_cast<DWORD>(-1), 0 };
	auto handle = AddVectoredExceptionHandler(1, PvectoredExceptionHandler);
	RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), reinterpret_cast<const ULONG_PTR*>(&info));
	RemoveVectoredExceptionHandler(handle);
}

const auto PSetThreadDescription = [] {
	auto proc = GetProcAddress(GetModuleHandle(EPRO_TEXT("kernel32.dll")), "SetThreadDescription");
	if(proc == nullptr)
		proc = GetProcAddress(GetModuleHandle(EPRO_TEXT("KernelBase.dll")), "SetThreadDescription");
	using SetThreadDescription_t = HRESULT(WINAPI*)(HANDLE, PCWSTR);
	return function_cast<SetThreadDescription_t>(proc);
}();

void NameThread(const char* name, const wchar_t* wname) {
	NameThreadMsvc(name);
	if(PSetThreadDescription)
		PSetThreadDescription(GetCurrentThread(), wname);
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

	auto* miniDumpWriteDumpFn = function_cast<MiniDumpWriteDump_t>(GetProcAddress(dbgHelpDLL, "MiniDumpWriteDump"));

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
	const auto dumpPath = epro::format(EPRO_TEXT("./crashdumps/EDOPro-pid{}-{}.mdmp"), (int)selfPid, (int)systemTicks);

	auto dumpFile = CreateFile(dumpPath.data(), GENERIC_WRITE, FILE_SHARE_WRITE,
							   nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
							   nullptr);

	if(dumpFile == INVALID_HANDLE_VALUE) {
		FreeLibrary(dbgHelpDLL);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if(miniDumpWriteDumpFn(selfProcHandle, selfPid, dumpFile, dumpType, &ExInfo, nullptr, nullptr))
		ygo::GUIUtils::ShowErrorWindow("Crash dump", epro::format("Succesfully wrote crash dump to file \"{}\"\n", ygo::Utils::ToUTF8IfNeeded(dumpPath)));

	CloseHandle(dumpFile);
	FreeLibrary(dbgHelpDLL);

	return EXCEPTION_CONTINUE_SEARCH;
}

}
#endif

namespace ygo {
	std::vector<SynchronizedIrrArchive> Utils::archives;
	irr::io::IFileSystem* Utils::filesystem{ nullptr };
	irr::ITimer* Utils::irrTimer{ nullptr };
	irr::IOSOperator* Utils::OSOperator{ nullptr };

	RNG::SplitMix64 Utils::generator(std::chrono::high_resolution_clock::now().time_since_epoch().count());

	void Utils::InternalSetThreadName(const char* name, [[maybe_unused]] const wchar_t* wname) {
#if EDOPRO_WINDOWS
		NameThread(name, wname);
#elif EDOPRO_LINUX_KERNEL
		pthread_setname_np(pthread_self(), name);
#elif EDOPRO_APPLE
		pthread_setname_np(name);
#endif //EDOPRO_WINDOWS
	}

	epro::thread::id Utils::GetCurrThreadId() {
		return epro::this_thread::get_id();
	}

#if !EDOPRO_ANDROID
	static auto main_thread_id = Utils::GetCurrThreadId();
#else
	extern epro::thread::id main_thread_id;
#endif

	epro::thread::id Utils::GetMainThreadId() {
		return main_thread_id;
	}

	void Utils::SetupCrashDumpLogging() {
#if EDOPRO_WINDOWS
		SetUnhandledExceptionFilter(crashDumpHandler);
#endif
	}

	bool Utils::IsRunningAsAdmin() {
#if EDOPRO_WINDOWS
		return false;
		// this code returned false positives
#if 0
		// https://stackoverflow.com/questions/8046097/how-to-check-if-a-process-has-the-administrative-rights
		HANDLE hToken = nullptr;
		if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
			return false;
		TOKEN_ELEVATION Elevation;
		DWORD cbSize = sizeof(TOKEN_ELEVATION);
		auto got_info = GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize);
		CloseHandle(hToken);
		if(got_info && Elevation.TokenIsElevated)
			return true;
#endif
#elif EDOPRO_LINUX || EDOPRO_MACOS
		auto uid = getuid();
		auto euid = geteuid();

		// if either effective uid or uid is the one of the root user assume running as root.
		// else if euid and uid are different then permissions errors can happen if its running
		// as a completly different user than the uid/euid
		if(uid == 0 || euid == 0 || uid != euid)
			return true;
#endif
		return false;
	}

	namespace {
	std::map<epro::thread::id, std::string> last_error_strings;
	epro::mutex last_error_strings_mutex;

	std::string& last_error_string_() {
		const auto id = epro::this_thread::get_id();
		std::unique_lock<epro::mutex> lk{ last_error_strings_mutex };
		auto it = last_error_strings.find(id);
		if(it != last_error_strings.end())
			return it->second;
		return last_error_strings[id];
	}
	}

	epro::stringview Utils::GetLastErrorString() {
		return last_error_string_();
	}

	static void SetLastError() {
		auto& last_error_string = last_error_string_();
#if EDOPRO_WINDOWS
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
#if EDOPRO_WINDOWS
		return SetLastErrorStringIfFailed(CreateDirectory(path.data(), nullptr) || ERROR_ALREADY_EXISTS == GetLastError());
#else
		return SetLastErrorStringIfFailed(mkdir(path.data(), 0777) == 0 || errno == EEXIST);
#endif
	}
	bool Utils::FileCopyFD([[maybe_unused]] int source, [[maybe_unused]] int destination) {
#if EDOPRO_LINUX_KERNEL
		off_t bytesCopied = 0;
		Stat fileinfo{};
		fstat(source, &fileinfo);
		int result = sendfile(destination, source, &bytesCopied, fileinfo.st_size);
		return SetLastErrorStringIfFailed(result != -1);
#elif EDOPRO_APPLE
		return SetLastErrorStringIfFailed(fcopyfile(source, destination, 0, COPYFILE_ALL) == 0);
#else
		return false;
#endif
	}
	bool Utils::FileCopy(epro::path_stringview source, epro::path_stringview destination) {
		if(source == destination)
			return false;
#if EDOPRO_WINDOWS
		return SetLastErrorStringIfFailed(CopyFile(source.data(), destination.data(), false));
#elif EDOPRO_LINUX_KERNEL
#if EDOPRO_ANDROID
#define NEEDS_FSTREAM
		if(!porting::pathIsUri(source) && !porting::pathIsUri(destination))
#endif
		{
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
		}
#elif EDOPRO_APPLE
		return SetLastErrorStringIfFailed(copyfile(source.data(), destination.data(), 0, COPYFILE_ALL) == 0);
#else
#define NEEDS_FSTREAM
#endif
#if defined(NEEDS_FSTREAM)
#undef NEEDS_FSTREAM
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
		if(source == destination)
			return true;
#if EDOPRO_WINDOWS
		return SetLastErrorStringIfFailed(MoveFile(source.data(), destination.data()));
#else
#if EDOPRO_ANDROID
		if(porting::pathIsUri(source) || porting::pathIsUri(destination)) {
			if(!FileCopy(source, destination))
				return false;
			if(!FileDelete(source)) {
				FileDelete(destination);
				return false;
			}
			return true;
		}
#endif
		return SetLastErrorStringIfFailed(rename(source.data(), destination.data()) == 0);
#endif
	}
	bool Utils::FileExists(epro::path_stringview path) {
#if EDOPRO_WINDOWS
		const auto dwAttrib = GetFileAttributes(path.data());
		return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
		Stat sb;
		return stat(path.data(), &sb) != -1 && S_ISREG(sb.st_mode) != 0;
#endif
	}
	static epro::path_string working_dir;
	bool Utils::SetWorkingDirectory(epro::path_stringview newpath) {
#if EDOPRO_WINDOWS
		bool res = SetLastErrorStringIfFailed(SetCurrentDirectory(newpath.data()));
#elif EDOPRO_IOS
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
	static epro::path_string user_storage_dir;
	void Utils::SetUserStorageDirectory(epro::path_stringview newpath) {
		user_storage_dir = NormalizePath(newpath);
	}
	const epro::path_string& Utils::GetUserStorageDirectory() {
		return user_storage_dir;
	}
	epro::path_string Utils::GetUserFolderPathFor(epro::path_stringview path) {
		return NormalizePath(epro::format(EPRO_TEXT("{}/{}"), GetUserStorageDirectory(), path));
	}
	bool Utils::FileDelete(epro::path_stringview source) {
#if EDOPRO_WINDOWS
		return SetLastErrorStringIfFailed(DeleteFile(source.data()));
#else
#if EDOPRO_ANDROID
		if(porting::pathIsUri(source))
			return porting::deleteFileUri(source);
#endif
		return SetLastErrorStringIfFailed(remove(source.data()) == 0);
#endif
	}

	void Utils::FindFiles(epro::path_stringview _path, const std::function<void(epro::path_stringview, bool)>& cb) {
		const auto path = Utils::NormalizePath(_path);
#if EDOPRO_WINDOWS
		WIN32_FIND_DATA fdata;
		auto fh = FindFirstFile(epro::format(EPRO_TEXT("{}*.*"), path).data(), &fdata);
		if(fh != INVALID_HANDLE_VALUE) {
			do {
				cb(fdata.cFileName, !!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
			} while(FindNextFile(fh, &fdata));
			FindClose(fh);
		}
#else
#if EDOPRO_ANDROID
		if(porting::pathIsUri(path)) {
			bool found_curdir = false;
			bool found_topdir = false;
			auto entries = porting::listFilesInFolder(path);
			for(auto& entry : entries) {
				bool isdir = entry.back() == EPRO_TEXT('/');
				if(isdir)
					entry.pop_back();
				cb(entry, isdir);
				if(entry == EPRO_TEXT("."sv))
					found_curdir = true;
				if(entry == EPRO_TEXT(".."sv))
					found_topdir = true;
			}
			if(!found_curdir)
				cb(EPRO_TEXT("."), true);
			if(!found_topdir)
				cb(EPRO_TEXT(".."), true);
		}
#endif
		if(auto dir = opendir(path.data())) {
#if EDOPRO_ANDROID
			// workaround, on android 11 (and probably higher) the folders "." and ".." aren't
			// returned by readdir, assuming the parsed path will never be root, manually
			// pass those 2 folders if they aren't returned by readdir
			bool found_curdir = false;
			bool found_topdir = false;
#endif //EDOPRO_ANDROID
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
#if EDOPRO_ANDROID
				if(dirp->d_name == EPRO_TEXT("."sv))
					found_curdir = true;
				if(dirp->d_name == EPRO_TEXT(".."sv))
					found_topdir = true;
#endif //EDOPRO_ANDROID
				cb(dirp->d_name, isdir);
			}
#if EDOPRO_ANDROID
			if(!found_curdir)
				cb(EPRO_TEXT("."), true);
			if(!found_topdir)
				cb(EPRO_TEXT(".."), true);
#endif //EDOPRO_ANDROID
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
	bool Utils::DeleteDirectory(epro::path_stringview path) {
		ClearDirectory(path);
#if EDOPRO_WINDOWS
		return RemoveDirectory(path.data());
#else
		return rmdir(path.data()) == 0;
#endif
	}
	bool Utils::DirectoryExists(epro::path_stringview path) {
#if EDOPRO_WINDOWS
		const auto dwAttrib = GetFileAttributes(path.data());
		return (dwAttrib != INVALID_FILE_ATTRIBUTES && ((dwAttrib & FILE_ATTRIBUTE_DIRECTORY) != 0));
#else
		Stat sb;
		return stat(path.data(), &sb) != -1 && S_ISDIR(sb.st_mode) != 0;
#endif
	}

	void Utils::CreateResourceFolders() {
		auto createResourceDirAndLogIfFailure = [](auto fn, epro::path_stringview path) {
			if(!fn(path)) {
				ygo::ErrorLog("Failed to create resource folder {} ({})", Utils::ToUTF8IfNeeded(path), GetLastErrorString());
			}
		};
		//create directories if missing
		createResourceDirAndLogIfFailure(MakeDirectory, DeckManager::GetDeckFolder());
		createResourceDirAndLogIfFailure(MakeDirectory, EPRO_TEXT("puzzles"));
		createResourceDirAndLogIfFailure(MakeDirectory, EPRO_TEXT("pics"));
		createResourceDirAndLogIfFailure(MakeDirectory, EPRO_TEXT("pics/field"));
		createResourceDirAndLogIfFailure(MakeDirectory, EPRO_TEXT("pics/cover"));
		createResourceDirAndLogIfFailure(MakeDirectory, EPRO_TEXT("pics/temp/"));
		createResourceDirAndLogIfFailure(ClearDirectory, EPRO_TEXT("pics/temp/"));
		createResourceDirAndLogIfFailure(MakeDirectory, Replay::GetReplayFolder());
		createResourceDirAndLogIfFailure(MakeDirectory, EPRO_TEXT("screenshots"));
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
#if EDOPRO_WINDOWS
		auto len = GetFullPathName(path.data(), 0, nullptr, nullptr);
		epro::path_string ret;
		ret.resize(len);
		len = GetFullPathName(path.data(), ret.size(), ret.data(), nullptr);
		ret.resize(len);
		std::replace(ret.begin(), ret.end(), EPRO_TEXT('\\'), EPRO_TEXT('/'));
		return ret;
#else
#if EDOPRO_ANDROID
		if(porting::pathIsUri(path))
			return epro::path_string{ path };
#endif
		char buff[PATH_MAX];
		if(realpath(path.data(), buff) == nullptr)
			return { path.data(), path.size() };
		return buff;
#endif
	}
	bool Utils::ContainsSubstring(epro::wstringview input, const std::vector<epro::wstringview>& tokens) {
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
#if EDOPRO_WINDOWS
		epro::path_string exepath;
		exepath.resize(32768);
		auto len = GetModuleFileName(nullptr, exepath.data(), exepath.size());
		exepath.resize(len);
		return Utils::NormalizePath(exepath, false);
#elif EDOPRO_LINUX
		epro::path_char buff[PATH_MAX];
		ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
		if(len != -1)
			buff[len] = EPRO_TEXT('\0');
		return buff;
#elif EDOPRO_MACOS
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
#if EDOPRO_WINDOWS
		return epro::format(EPRO_TEXT("{}/ocgcore.dll"), Utils::GetExeFolder());
#else
		return EPRO_TEXT(""); // Unused on POSIX
#endif
	}();
	const epro::path_string& Utils::GetCorePath() {
		return core_path;
	}

	bool Utils::UnzipArchive(epro::path_stringview input, unzip_callback callback, unzip_payload* payload, epro::path_stringview dest) {
		std::vector<char> buff;
		buff.resize(0x2000);
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
						int wx = static_cast<int>(reader->read(buff.data(), buff.size()));
						out.write(buff.data(), wx);
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
#if EDOPRO_WINDOWS
		if(type == SHARE_FILE)
			return;
		ShellExecute(nullptr, EPRO_TEXT("open"), (type == OPEN_FILE) ? epro::format(EPRO_TEXT("{}/{}"), GetWorkingDirectory(), arg).data() : arg.data(), nullptr, nullptr, SW_SHOWNORMAL);
#elif EDOPRO_MACOS || EDOPRO_LINUX
		if(type == SHARE_FILE)
			return;
#if EDOPRO_MACOS
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
#elif EDOPRO_ANDROID
		switch(type) {
		case OPEN_FILE:
			if(porting::pathIsUri(arg))
				return porting::openFile(arg);
			return porting::openFile(epro::format("{}/{}", GetWorkingDirectory(), arg));
		case OPEN_URL:
			return porting::openUrl(arg);
		case SHARE_FILE:
			if(porting::pathIsUri(arg))
				return porting::shareFile(arg);
			return porting::shareFile(epro::format("{}/{}", GetWorkingDirectory(), arg));
		}
#endif
	}

	void Utils::Reboot() {
#if EDOPRO_WINDOWS || EDOPRO_LINUX || EDOPRO_MACOS
		const auto& path = GetExePath();
#if EDOPRO_WINDOWS
		STARTUPINFO si{ sizeof(si) };
		PROCESS_INFORMATION pi{};
		auto command = epro::format(EPRO_TEXT("{} -C \"{}\" -l"), GetFileName(path, true), GetWorkingDirectory());
		if(!CreateProcess(path.data(), &command[0], nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi))
			return;
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
#else
#if EDOPRO_LINUX
		struct stat fileStat;
		stat(path.data(), &fileStat);
		chmod(path.data(), fileStat.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
#endif
		{
			[[maybe_unused]] const auto* path_cstr = path.data();
			const auto& workdir = GetWorkingDirectory();
			const auto* workdir_cstr = workdir.data();
			auto pid = vfork();
			if(pid == 0) {
#if EDOPRO_LINUX
				execl(path_cstr, path_cstr, "-C", workdir_cstr, "-l", nullptr);
#else
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
