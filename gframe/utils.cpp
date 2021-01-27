#include "utils.h"
#include <cmath> // std::round
#include <fstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h> // ShellExecute
#else
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
using Stat = struct stat;
using Dirent = struct dirent;
#endif
#ifdef __APPLE__
#import <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>
#include <CoreServices/CoreServices.h>
#include <copyfile.h>
#endif
#ifdef __ANDROID__
#include "Android/porting_android.h"
#endif
#ifdef __linux__
#include <sys/sendfile.h>
#endif
#include <IFileArchive.h>
#include <IFileSystem.h>
#include <fmt/format.h>
#include <IOSOperator.h>
#include "config.h"
#include "bufferio.h"

#if defined(_WIN32) && defined(_MSC_VER)
namespace WindowsWeirdStuff {

//https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code?view=vs-2015&redirectedfrom=MSDN

constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;
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
void NameThread(const char* threadName) {
	const THREADNAME_INFO info{ 0x1000, threadName, ((DWORD)-1), 0 };
	__try {	RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info); }
	__except(EXCEPTION_EXECUTE_HANDLER) {}
}
}
#pragma warning(pop)
#endif

namespace ygo {
	std::vector<SynchronizedIrrArchive> Utils::archives;
	irr::io::IFileSystem* Utils::filesystem{ nullptr };
	irr::IOSOperator* Utils::OSOperator{ nullptr };
	epro::path_string Utils::working_dir;

	void Utils::InternalSetThreadName(const char* name, const wchar_t* wname) {
#if defined(_WIN32)
		static const auto PSetThreadDescription = (HRESULT(WINAPI *)(HANDLE, PCWSTR))GetProcAddress(GetModuleHandle(EPRO_TEXT("kernel32.dll")), "SetThreadDescription");
		if(PSetThreadDescription)
			PSetThreadDescription(GetCurrentThread(), wname);
#if defined(_MSC_VER)
		WindowsWeirdStuff::NameThread(name);
#endif //_MSC_VER
#else
		(void)wname;
#if defined(__linux__)
		pthread_setname_np(pthread_self(), name);
#elif defined(__APPLE__)
		pthread_setname_np(name);
#endif //__linux__
#endif //_WIN32
	}

	bool Utils::MakeDirectory(epro::path_stringview path) {
#ifdef _WIN32
		return CreateDirectory(path.data(), NULL) || ERROR_ALREADY_EXISTS == GetLastError();
#else
		return mkdir(path.data(), 0777) == 0 || errno == EEXIST;
#endif
	}
	bool Utils::FileCopy(epro::path_stringview source, epro::path_stringview destination) {
		if(source == destination)
			return false;
#ifdef _WIN32
		return CopyFile(source.data(), destination.data(), false);
#elif defined(__linux__)
		int input, output;
		if((input = open(source.data(), O_RDONLY)) == -1) {
			return false;
		}
		if((output = creat(destination.data(), 0660)) == -1) {
			close(input);
			return false;
		}
		off_t bytesCopied = 0;
		struct stat fileinfo = { 0 };
		fstat(input, &fileinfo);
		int result = sendfile(output, input, &bytesCopied, fileinfo.st_size);
		close(input);
		close(output);
		return result != -1;
#elif defined(__APPLE__)
		return copyfile(source.data(), destination.data(), 0, COPYFILE_ALL) == 0;
#else
		std::ifstream src(source.data(), std::ios::binary);
		if(!src.is_open())
			return false;
		std::ofstream dst(destination.data(), std::ios::binary);
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
		return MoveFile(source.data(), destination.data());
#else
		return rename(source.data(), destination.data()) == 0;
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
	bool Utils::ChangeDirectory(epro::path_stringview newpath) {
#ifdef _WIN32
		return SetCurrentDirectory(newpath.data());
#else
		return chdir(newpath.data()) == 0;
#endif
	}
	bool Utils::FileDelete(epro::path_stringview source) {
#ifdef _WIN32
		return DeleteFile(source.data());
#else
		return remove(source.data()) == 0;
#endif
	}

	void Utils::FindFiles(epro::path_stringview path, const std::function<void(epro::path_stringview, bool)>& cb) {
#ifdef _WIN32
		WIN32_FIND_DATA fdata;
		HANDLE fh = FindFirstFile(fmt::format(EPRO_TEXT("{}*.*"), NormalizePath<epro::path_string>({ path.data(), path.size() })).data(), &fdata);
		if(fh != INVALID_HANDLE_VALUE) {
			do {
				cb(fdata.cFileName, !!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
			} while(FindNextFile(fh, &fdata));
			FindClose(fh);
		}
#else
		DIR* dir = nullptr;
		Dirent* dirp = nullptr;
		auto _path = NormalizePath<epro::path_string>({ path.data(), path.size() });
		if((dir = opendir(_path.data())) != nullptr) {
			while((dirp = readdir(dir)) != nullptr) {
#ifdef _DIRENT_HAVE_D_TYPE //avoid call to format and stat
				const bool isdir = dirp->d_type == DT_DIR;
#else
				Stat fileStat;
				stat(fmt::format("{}{}", _path, dirp->d_name).data(), &fileStat);
				const bool isdir = !!S_ISDIR(fileStat.st_mode);
#endif
				cb(dirp->d_name, isdir);
			}
			closedir(dir);
		}
#endif
	}

#define ROOT_OR_CUR(str) (str == EPRO_TEXT(".") || (str == EPRO_TEXT("..")))
	bool Utils::ClearDirectory(epro::path_stringview path) {
		FindFiles(path, [&path](epro::path_stringview name, bool isdir) {
			if(isdir) {
				if(!ROOT_OR_CUR(name))
					DeleteDirectory(fmt::format(EPRO_TEXT("{}{}/"), path, name));
			} else
				FileDelete(fmt::format(EPRO_TEXT("{}{}"), path, name));
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
					auto res2 = FindFiles(fmt::format(EPRO_TEXT("{}{}/"), path, name), extensions, subdirectorylayers - 1);
					for(auto& file : res2)
						file = fmt::format(EPRO_TEXT("{}/{}"), name, file);
					res.insert(res.end(), std::make_move_iterator(res2.begin()), std::make_move_iterator(res2.end()));
				}
			} else {
				if(extensions.empty() || std::find(extensions.begin(), extensions.end(), Utils::GetFileExtension<epro::path_string>({ name.data(), name.size() })) != extensions.end())
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
			epro::path_string fullpath = fmt::format(EPRO_TEXT("{}{}/"), path, name);
			epro::path_stringview cur = name;
			if(addparentpath)
				cur = fullpath;
			results.push_back({ cur.data(), cur.size() });
			if(subdirectorylayers > 1) {
				auto subresults = FindSubfolders(fullpath, subdirectorylayers - 1, false);
				for(auto& folder : subresults) {
					folder = fmt::format(EPRO_TEXT("{}{}/"), fullpath, folder);
				}
				results.insert(results.end(), std::make_move_iterator(subresults.begin()), std::make_move_iterator(subresults.end()));
			}
		});
		std::sort(results.begin(), results.end(), CompareIgnoreCase<epro::path_string>);
		return results;
	}
	std::vector<int> Utils::FindFiles(irr::io::IFileArchive* archive, epro::path_stringview path, const std::vector<epro::path_stringview>& extensions, int subdirectorylayers) {
		std::vector<int> res;
		auto list = archive->getFileList();
		for(irr::u32 i = 0; i < list->getFileCount(); i++) {
			if(list->isDirectory(i))
				continue;
			const auto name = list->getFullFileName(i);
			if(std::count(name.c_str(), name.c_str() + name.size(), EPRO_TEXT('/')) > subdirectorylayers)
				continue;
			if(extensions.empty() || std::find(extensions.begin(), extensions.end(), Utils::GetFileExtension<epro::path_string>({ name.c_str(), name.size() })) != extensions.end())
				res.push_back(i);
		}
		return res;
	}
	MutexLockedIrrArchivedFile::~MutexLockedIrrArchivedFile() {
		if (reader)
			reader->drop();
		if (mutex)
			mutex->unlock();
	}
	MutexLockedIrrArchivedFile Utils::FindFileInArchives(epro::path_stringview path, epro::path_stringview name) {
		for(auto& archive : archives) {
			archive.mutex->lock();
			int res = -1;
			auto list = archive.archive->getFileList();
			res = list->findFile(fmt::format(EPRO_TEXT("{}{}"), path, name).data());
			if(res != -1) {
				auto reader = archive.archive->createAndOpenFile(res);
				if(reader)
					return MutexLockedIrrArchivedFile(archive.mutex.get(), reader); // drops reader and unlocks when done
			}
			archive.mutex->unlock();
		}
		return MutexLockedIrrArchivedFile(); // file not found
	}
	epro::stringview Utils::GetUserAgent() {
		static const std::string agent = fmt::format("EDOPro-" OSSTRING "-" STR(EDOPRO_VERSION_MAJOR) "." STR(EDOPRO_VERSION_MINOR) "." STR(EDOPRO_VERSION_PATCH)" {}",
											   ygo::Utils::OSOperator->getOperatingSystemVersion().c_str());
		return agent;
	}
	bool Utils::ContainsSubstring(epro::wstringview input, const std::vector<std::wstring>& tokens, bool convertInputCasing, bool convertTokenCasing) {
		static std::vector<std::wstring> alttokens;
		static std::wstring casedinput;
		if (input.empty() || tokens.empty())
			return false;
		if(convertInputCasing) {
			casedinput = ToUpperNoAccents<std::wstring>({ input.data(), input.size() });
			input = casedinput;
		}
		if (convertTokenCasing) {
			alttokens.clear();
			for (const auto& token : tokens)
				alttokens.push_back(ToUpperNoAccents(token));
		}
		std::size_t pos1, pos2 = 0;
		for (auto& token : convertTokenCasing ? alttokens : tokens) {
			if ((pos1 = input.find(token, pos2)) == epro::wstringview::npos)
				return false;
			pos2 = pos1 + token.size();
		}
		return true;
	}
	bool Utils::ContainsSubstring(epro::wstringview input, epro::wstringview token, bool convertInputCasing, bool convertTokenCasing) {
		if (input.empty() && !token.empty())
			return false;
		if (token.empty())
			return true;
		if(convertInputCasing) {
			return ToUpperNoAccents<std::wstring>(input.data()).find(convertTokenCasing ? ToUpperNoAccents<std::wstring>(token.data()).data() : token.data()) != std::wstring::npos;
		} else {
			return input.find(convertTokenCasing ? ToUpperNoAccents<std::wstring>(token.data()).data() : token.data()) != epro::wstringview::npos;
		}
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

	epro::path_stringview Utils::GetExePath() {
		static epro::path_string binarypath = []()->epro::path_string {
#ifdef _WIN32
			TCHAR exepath[MAX_PATH];
			GetModuleFileName(NULL, exepath, MAX_PATH);
			return Utils::NormalizePath<epro::path_string>(exepath, false);
#elif defined(__linux__) && !defined(__ANDROID__)
			epro::path_char buff[PATH_MAX];
			ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
			if(len != -1)
				buff[len] = EPRO_TEXT('\0');
			return buff;
#elif defined(__APPLE__)
			CFURLRef bundle_url = CFBundleCopyBundleURL(CFBundleGetMainBundle());
			CFStringRef bundle_path = CFURLCopyFileSystemPath(bundle_url, kCFURLPOSIXPathStyle);
			CFURLRef bundle_base_url = CFURLCreateCopyDeletingLastPathComponent(NULL, bundle_url);
			CFRelease(bundle_url);
			CFStringRef path = CFURLCopyFileSystemPath(bundle_base_url, kCFURLPOSIXPathStyle);
			CFRelease(bundle_base_url);
			/*
			#ifdef MAC_OS_DISCORD_LAUNCHER
				system(fmt::format("open {}/Contents/MacOS/discord-launcher.app --args random", CFStringGetCStringPtr(bundle_path, kCFStringEncodingUTF8)).c_str());
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
		return binarypath;
	}

	epro::path_stringview Utils::GetExeFolder() {
		static epro::path_string binarypath = GetFilePath(GetExePath().to_string());
		return binarypath;
	}

	epro::path_stringview Utils::GetCorePath() {
		static epro::path_string binarypath = [] {
#ifdef _WIN32
			return fmt::format(EPRO_TEXT("{}/ocgcore.dll"), GetExeFolder());
#else
			return EPRO_TEXT(""); // Unused on POSIX
#endif
		}();
		return binarypath;
	}

	bool Utils::UnzipArchive(epro::path_stringview input, unzip_callback callback, unzip_payload* payload, epro::path_stringview dest) {
		thread_local char buff[0x2000];
		constexpr int buff_size = sizeof(buff) / sizeof(*buff);
		if(!filesystem)
			return false;
		CreatePath(dest, EPRO_TEXT("./"));
		irr::io::IFileArchive* archive = nullptr;
		////////kdiy////////
		//if(!filesystem->addFileArchive({ input.data(), (irr::u32)input.size() }, false, false, irr::io::EFAT_ZIP, "", &archive))
		    //return false;
		#ifdef Zip
		if(!filesystem->addFileArchive({ input.data(), (irr::u32)input.size() }, false, false, irr::io::EFAT_ZIP, Zip, &archive))
		    return false;
		#else
		if(!filesystem->addFileArchive({ input.data(), (irr::u32)input.size() }, false, false, irr::io::EFAT_ZIP, "", &archive))
		    return false;
		#endif
		////////kdiy////////		

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
			epro::path_stringview filename = filelist->getFullFileName(i).c_str();
			bool isdir = filelist->isDirectory(i);
			if(isdir)
				CreatePath(fmt::format(EPRO_TEXT("{}/"), filename), { dest.data(), dest.size() });
			else
				CreatePath(filename, { dest.data(), dest.size() });
			if(!isdir) {
				int percentage = 0;
				auto reader = archive->createAndOpenFile(i);
				if(reader) {
					std::ofstream out(fmt::format(EPRO_TEXT("{}/{}") , dest, filename), std::ofstream::binary);
					int r, rx = reader->getSize();
					if(payload) {
						payload->is_new = true;
						payload->cur = i;
						payload->percentage = 0;
						payload->filename = filename.data();
						callback(payload);
						payload->is_new = false;
					}
					for(r = 0; r < rx; /**/) {
						int wx = reader->read(buff, buff_size);
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
					out.close();
					reader->drop();
				}
			}
		}
		filesystem->removeFileArchive(archive);
		archive->drop();
		return true;
	}

	void Utils::SystemOpen(epro::path_stringview url, OpenType type) {
#ifdef _WIN32
		ShellExecute(NULL, EPRO_TEXT("open"), (type == OPEN_FILE) ? fmt::format(EPRO_TEXT("{}/{}"), working_dir, url).data() : url.data(), NULL, NULL, SW_SHOWNORMAL);
#elif !defined(__ANDROID__)
		auto pid = vfork();
		if(pid == 0) {
#ifdef __APPLE__
			execl("/usr/bin/open", "open", url.data(), NULL);
#else
			execl("/usr/bin/xdg-open", "xdg-open", url.data(), NULL);
#endif
			perror("Failed to open browser:");
		} else if(pid < 0) {
			perror("Failed to fork:");
		}
#else
		if(type == OPEN_FILE)
			porting::openFile(fmt::format("{}/{}", working_dir, url));
		else
			porting::openUrl(url);
#endif
	}
}

