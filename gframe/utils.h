#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <functional>
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#endif

#ifdef UNICODE
using path_char = wchar_t;
#else
using path_char = char;
#endif
using path_string = std::basic_string<path_char>;

namespace ygo {
	class Utils {
	public:
		static bool Makedirectory(const path_string& path);
		static bool Movefile(const path_string& source, const path_string& destination);
		static path_string ParseFilename(const std::wstring& input);
		static path_string ParseFilename(const std::string& input);
		static std::string ToUTF8IfNeeded(const path_string& input);
		static std::wstring ToUnicodeIfNeeded(const path_string& input);
		static bool Deletefile(const path_string& source);
		static bool ClearDirectory(const path_string& path);
		static bool Deletedirectory(const path_string& source);
		static void FindfolderFiles(const path_string& path, const std::function<void(path_string, bool, void*)>& cb, void* payload = nullptr);
		static std::vector<path_string> FindfolderFiles(const path_string& path, std::vector<path_string> extensions, int subdirectorylayers = 0);
		static std::wstring GetFileExtension(std::wstring file);
		static std::string GetFileExtension(std::string file);
	};
}

#endif //UTILS_H
