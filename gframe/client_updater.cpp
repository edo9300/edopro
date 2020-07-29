#include "client_updater.h"
#ifdef UPDATE_URL
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif // _WIN32
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <fstream>
#include <atomic>
#include <openssl/md5.h>
#include "config.h"
#include "utils.h"

struct Payload {
	update_callback callback = nullptr;
	int current = 1;
	int total = 1;
	bool is_new = true;
	int previous_percent = 0;
	void* payload = nullptr;
	const char* filename = nullptr;
};

int progress_callback(void* ptr, curl_off_t TotalToDownload, curl_off_t NowDownloaded, curl_off_t TotalToUpload, curl_off_t NowUploaded) {
	Payload* payload = reinterpret_cast<Payload*>(ptr);
	if(payload && payload->callback) {
		int percentage = 0;
		if(TotalToDownload > 0.0) {
			double fractiondownloaded = (double)NowDownloaded / (double)TotalToDownload;
			percentage = std::round(fractiondownloaded * 100);
		}
		if(percentage != payload->previous_percent) {
			payload->callback(percentage, payload->current, payload->total, payload->filename, payload->is_new, payload->payload);
			payload->is_new = false;
			payload->previous_percent = percentage;
		}
	}
	return 0;
}

size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	auto buff = static_cast<std::vector<char>*>(userp);
	size_t prev_size = buff->size();
	buff->resize(prev_size + realsize);
	memcpy((char*)buff->data() + prev_size, contents, realsize);
	return realsize;
}

CURLcode curlPerform(const char* url, void* payload, void* payload2 = nullptr) {
	CURL* curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, reinterpret_cast<void*>(WriteCallback));
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, payload);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, EDOPRO_USERAGENT);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROXY, "*");
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl_handle, CURLOPT_DNS_CACHE_TIMEOUT, 0);
	curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, progress_callback);
	curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, payload2);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
	CURLcode res = curl_easy_perform(curl_handle);
	curl_easy_cleanup(curl_handle);
	return res;
}

void Reboot() {
#ifdef _WIN32
	STARTUPINFO si{ sizeof(STARTUPINFO) };
	PROCESS_INFORMATION pi{};
	auto pathstring = ygo::Utils::GetExePath() + EPRO_TEXT(" show_changelog");
	CreateProcess(nullptr,
		(TCHAR*)pathstring.c_str(),
				  nullptr,
				  nullptr,
				  false,
				  0,
				  nullptr,
				  EPRO_TEXT("./"),
				  &si,
				  &pi);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
#elif defined(__APPLE__)
	system("open -b io.github.edo9300.ygoprodll --args show_changelog");
#else
	pid_t pid = fork();
	if(pid == 0) {
		struct stat fileStat;
		stat(ygo::Utils::GetExePath().c_str(), &fileStat);
		chmod(ygo::Utils::GetExePath().c_str(), fileStat.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
		execl(ygo::Utils::GetExePath().c_str(), "show_changelog", nullptr);
		exit(EXIT_FAILURE);
	}
#endif
	exit(0);
}

bool CheckMd5(const std::vector<char>& buffer, const std::vector<uint8_t>& md5) {
	if(md5.size() < MD5_DIGEST_LENGTH)
		return false;
	uint8_t result[MD5_DIGEST_LENGTH];
	MD5((uint8_t*)buffer.data(), buffer.size(), result);
	return memcmp(result, md5.data(), MD5_DIGEST_LENGTH) == 0;
}

void DeleteOld() {
#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))
	ygo::Utils::FileDelete(ygo::Utils::GetExePath() + EPRO_TEXT(".old"));
#if !defined(__linux__)
	ygo::Utils::FileDelete(ygo::Utils::GetCorePath() + EPRO_TEXT(".old"));
#endif
#endif
}

ygo::ClientUpdater::lock_type GetLock() {
#ifdef _WIN32
	HANDLE hFile = CreateFile(EPRO_TEXT("./.edopro_lock"),
							  GENERIC_READ,
							  0,
							  nullptr,
							  CREATE_ALWAYS,
							  FILE_ATTRIBUTE_HIDDEN,
							  nullptr);
	if(!hFile || hFile == INVALID_HANDLE_VALUE)
		return nullptr;
	DeleteOld();
	return hFile;
#else
	size_t file = open("./.edopro_lock", O_CREAT, S_IRWXU);
	if(flock(file, LOCK_EX | LOCK_NB)) {
		if(file)
			close(file);
		return 0;
	}
	DeleteOld();
	return file;
#endif
	return 0;
}

void FreeLock(ygo::ClientUpdater::lock_type lock) {
	if(!lock)
		return;
#ifdef _WIN32
	CloseHandle(lock);
#else
	flock(lock, LOCK_UN);
	close(lock);
#endif
	ygo::Utils::FileDelete(EPRO_TEXT("./.edopro_lock"));
}
#endif
namespace ygo {

void ClientUpdater::StartUnzipper(unzip_callback callback, void* payload, const path_string& src) {
#ifdef UPDATE_URL
	if(Lock)
		std::thread(&ClientUpdater::Unzip, this, src, payload, callback).detach();
#endif
}

void ClientUpdater::CheckUpdates() {
#ifdef UPDATE_URL
	if(Lock)
		std::thread(&ClientUpdater::CheckUpdate, this).detach();
#endif
}

bool ClientUpdater::StartUpdate(update_callback callback, void* payload, const path_string& dest) {
#ifdef UPDATE_URL
	if(!has_update || downloading || !Lock)
		return false;
	std::thread(&ClientUpdater::DownloadUpdate, this, dest, payload, callback).detach();
	return true;
#else
	return false;
#endif
}
#ifdef UPDATE_URL
void ClientUpdater::Unzip(path_string src, void* payload, unzip_callback callback) {
#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))
	auto& path = ygo::Utils::GetExePath();
	ygo::Utils::FileMove(path, path + EPRO_TEXT(".old"));
#if !defined(__linux__)
	auto& corepath = ygo::Utils::GetExePath();
	ygo::Utils::FileMove(corepath, corepath + EPRO_TEXT(".old"));
#endif
#endif
	unzip_payload cbpayload{};
	UnzipperPayload uzpl;
	uzpl.payload = payload;
	uzpl.cur = -1;
	uzpl.tot = static_cast<int>(update_urls.size());
	cbpayload.payload = &uzpl;
	int i = 1;
	for(auto& file : update_urls) {
		uzpl.cur = i++;
		auto name = src + ygo::Utils::ToPathString(file.name);
		uzpl.filename = name.c_str();
		ygo::Utils::UnzipArchive(name, callback, &cbpayload);
	}
	Reboot();
}

void ClientUpdater::DownloadUpdate(path_string dest_path, void* payload, update_callback callback) {
	downloading = true;
	Payload cbpayload{};
	cbpayload.callback = callback;
	cbpayload.total = static_cast<int>(update_urls.size());
	cbpayload.payload = payload;
	int i = 1;
	for(auto& file : update_urls) {
		auto name = dest_path + EPRO_TEXT("/") + ygo::Utils::ToPathString(file.name);
		cbpayload.current = i++;
		cbpayload.filename = file.name.data();
		cbpayload.is_new = true;
		cbpayload.previous_percent = -1;
		std::vector<uint8_t> binmd5;
		for(std::string::size_type i = 0; i < file.md5.length(); i += 2) {
			uint8_t b = static_cast<uint8_t>(std::stoul(file.md5.substr(i, 2), nullptr, 16));
			binmd5.push_back(b);
		}
		std::fstream stream;
		stream.open(name, std::fstream::in | std::fstream::binary);
		if(stream.good() && CheckMd5({ std::istreambuf_iterator<char>(stream),
									  std::istreambuf_iterator<char>() }, binmd5))
			continue;
		stream.close();
		std::vector<char> buffer;
		if((curlPerform(file.url.c_str(), &buffer, &cbpayload) != CURLE_OK)
		   || !CheckMd5(buffer, binmd5)
		   || !ygo::Utils::CreatePath(name))
			continue;
		stream.open(name, std::fstream::out | std::fstream::trunc | std::ofstream::binary);
		if(stream.good())
			stream.write(buffer.data(), buffer.size());
	}
	downloaded = true;
}

void ClientUpdater::CheckUpdate() {
	std::vector<char> retrieved_data;
	if(curlPerform(UPDATE_URL, &retrieved_data) != CURLE_OK)
		return;
	try {
		nlohmann::json j = nlohmann::json::parse(retrieved_data);
		if(!j.is_array())
			return;
		for(auto& asset : j) {
			try {
				auto url = asset["url"].get<std::string>();
				auto name = asset["name"].get<std::string>();
				auto md5 = asset["md5"].get<std::string>();
				update_urls.push_back(std::move(DownloadInfo{ std::move(name),
										 std::move(url),
										 std::move(md5) }));
			}
			catch(...) {}
		}
	}
	catch(...) { update_urls.clear(); }
	has_update = !!update_urls.size();
}
#endif

ClientUpdater::ClientUpdater() {
#ifdef UPDATE_URL
	Lock = GetLock();
#endif
}

ClientUpdater::~ClientUpdater() {
#ifdef UPDATE_URL
	FreeLock(Lock);
#endif
}

};
