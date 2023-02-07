#ifndef IMAGEMANAGER_H
#define IMAGEMANAGER_H

#include "config.h"
#include <path.h>
#include <rect.h>
#include <unordered_map>
#include <map>
#include <atomic>
#include <queue>
#include "epro_mutex.h"
#include "epro_condition_variable.h"
#include "epro_thread.h"

namespace irr {
class IrrlichtDevice;
namespace io {
class IReadFile;
}
namespace video {
class IImage;
class ITexture;
class IVideoDriver;
class SColor;
}
}

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

class ImageManager {
private:
	using chrono_time = uint64_t;
	enum class preloadStatus {
		NONE,
		LOADING,
		LOADED,
		WAIT_DOWNLOAD,
	};
	struct texture_map_entry {
		preloadStatus preload_status;
		irr::video::ITexture* texture;
	};
	using texture_map = std::unordered_map<uint32_t, texture_map_entry>;
	struct load_parameter {
		uint32_t code;
		imgType type;
		size_t index;
		const std::atomic<irr::s32>& reference_width;
		const std::atomic<irr::s32>& reference_height;
		chrono_time timestamp;
		const std::atomic<chrono_time>& reference_timestamp;
		load_parameter(uint32_t code_, imgType type_, size_t index_, const std::atomic<irr::s32>& reference_width_,
					   const std::atomic<irr::s32>& reference_height_, chrono_time timestamp_, const std::atomic<chrono_time>& reference_timestamp_) :
			code(code_), type(type_), index(index_), reference_width(reference_width_),
			reference_height(reference_height_), timestamp(timestamp_),
			reference_timestamp(reference_timestamp_) {
		}
	};
	enum class loadStatus {
		LOAD_OK,
		LOAD_FAIL,
		WAIT_DOWNLOAD,
	};
	struct load_return {
		loadStatus status;
		uint32_t code;
		irr::video::IImage* texture;
		epro::path_string path;
	};
public:
	ImageManager();
	~ImageManager();
	bool Initial();
	void ChangeTextures(epro::path_stringview path);
	void ResetTextures();
	void SetDevice(irr::IrrlichtDevice* dev);
	void ClearTexture(bool resize = false);
	void RefreshCachedTextures();
	void ClearCachedTextures();
	static bool imageScaleNNAA(irr::video::IImage* src, irr::video::IImage* dest, chrono_time timestamp_id, const std::atomic<chrono_time>& source_timestamp_id);
	irr::video::IImage* GetScaledImage(irr::video::IImage* srcimg, int width, int height, chrono_time timestamp_id, const std::atomic<chrono_time>& source_timestamp_id);
	irr::video::IImage* GetScaledImageFromFile(const irr::io::path& file, int width, int height);
	irr::video::ITexture* GetTextureFromFile(const irr::io::path& file, int width, int height);
	irr::video::ITexture* GetTextureCard(uint32_t code, imgType type, bool wait = false, bool fit = false, int* chk = nullptr);
	irr::video::ITexture* GetTextureField(uint32_t code);
	irr::video::ITexture* GetCheckboxScaledTexture(float scale);
	irr::video::ITexture* guiScalingResizeCached(irr::video::ITexture* src, const irr::core::rect<irr::s32>& srcrect,
												 const irr::core::rect<irr::s32> &destrect);
	void draw2DImageFilterScaled(irr::video::ITexture* txr,
								 const irr::core::rect<irr::s32>& destrect, const irr::core::rect<irr::s32>& srcrect,
								 const irr::core::rect<irr::s32>* cliprect = nullptr, const irr::video::SColor* const colors = nullptr,
								 bool usealpha = false);
private:
	texture_map tMap[2];
	texture_map tThumb;
	std::unordered_map<uint32_t, irr::video::ITexture*> tFields;
	texture_map tCovers;
	irr::IrrlichtDevice* device;
	irr::video::IVideoDriver* driver;
public:
	irr::video::ITexture* tCover[2];
	irr::video::ITexture* tUnknown;
#define A(what) \
		public: \
		irr::video::ITexture* what;\
		private: \
		irr::video::ITexture* def_##what;
	A(tAct)
	A(tAttack)
	A(tNegated)
	A(tChain)
	A(tNumber)
	A(tLPFrame)
	A(tLPBar)
	A(tMask)
	A(tEquip)
	A(tTarget)
	A(tChainTarget)
	A(tLim)
	A(tOT)
	A(tHand[3])
	A(tBackGround)
	A(tBackGround_menu)
	A(tBackGround_deck)
	A(tBackGround_duel_topdown)
	A(tField[2][4])
	A(tFieldTransparent[2][4])
	A(tSettings)
	A(tCheckBox[3])
#undef A
private:
	void ClearFutureObjects();
	void RefreshCovers();
	void LoadPic();
	irr::video::ITexture* loadTextureFixedSize(epro::path_stringview texture_name, int width, int height);
	irr::video::ITexture* loadTextureAnySize(epro::path_stringview texture_name);
	void replaceTextureLoadingFixedSize(irr::video::ITexture*& texture, irr::video::ITexture* fallback, epro::path_stringview texture_name, int width, int height);
	void replaceTextureLoadingAnySize(irr::video::ITexture*& texture, irr::video::ITexture* fallback, epro::path_stringview texture_name);
	load_return LoadCardTexture(uint32_t code, imgType type, const std::atomic<irr::s32>& width, const std::atomic<irr::s32>& height, chrono_time timestamp_id, const std::atomic<chrono_time>& source_timestamp_id);
	epro::path_string textures_path;
	std::pair<std::atomic<irr::s32>, std::atomic<irr::s32>> sizes[3];
	std::atomic<chrono_time> timestamp_id;
	std::map<epro::path_string, irr::video::ITexture*> g_txrCache;
	std::map<irr::io::path, irr::video::IImage*> g_imgCache; //ITexture->getName returns a io::path
	epro::mutex obj_clear_lock;
	epro::thread obj_clear_thread;
	epro::condition_variable cv_clear;
	std::deque<load_return> to_clear;
	std::atomic<bool> stop_threads;
	epro::condition_variable cv_load;
	std::deque<load_parameter> to_load;
	std::deque<load_return> loaded_pics[4];
	epro::mutex pic_load;
	//bool stop_threads;
	std::vector<epro::thread> load_threads;
};

#define CARD_IMG_WIDTH		177
#define CARD_IMG_HEIGHT		254
#define CARD_IMG_WIDTH_F	177.0f
#define CARD_IMG_HEIGHT_F	254.0f
#define CARD_THUMB_WIDTH	44
#define CARD_THUMB_HEIGHT	64

}

#endif // IMAGEMANAGER_H
