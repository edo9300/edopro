#ifndef IMAGE_DOWNLOADER_H
#define IMAGE_DOWNLOADER_H
#include <memory>
#include "epro_mutex.h"
#include "epro_condition_variable.h"
#include "epro_thread.h"
#include <vector>
#include <atomic>
#include <deque>
#include <map>
#include "text_types.h"

namespace ygo {
#ifndef IMGTYPE
#define IMGTYPE
enum imgType {
	ART,
	FIELD,
	COVER,
	THUMB
};
#endif

class ImageDownloader {
public:
	enum class downloadStatus {
		DOWNLOADING,
		DOWNLOAD_ERROR,
		DOWNLOADED,
		NONE
	};
	struct PicSource {
		std::string url;
		imgType type;
	};
	ImageDownloader();
	~ImageDownloader();
	void AddDownloadResource(PicSource src);
	downloadStatus GetDownloadStatus(uint32_t code, imgType type);
	epro::path_stringview GetDownloadPath(uint32_t code, imgType type);
	void AddToDownloadQueue(uint32_t code, imgType type);
private:
	struct downloadParam {
		uint32_t code;
		imgType type;
	};
	struct downloadReturn {
		downloadStatus status;
		epro::path_string path;
	};
	using downloading_map = std::map<uint32_t/*code*/, downloadReturn>; /*if the value is not found, the download hasn't started yet*/
	void DownloadPic();
	downloading_map downloading_images[3];
	std::deque<downloadParam> to_download;
	std::pair<std::atomic<int>, std::atomic<int>> sizes[3];
	epro::mutex pic_download;
	epro::condition_variable cv;
	bool stop_threads;
	std::vector<PicSource> pic_urls;
	std::vector<epro::thread> download_threads;
};

extern ImageDownloader* gImageDownloader;

}


#endif //IMAGE_DOWNLOADER_H
