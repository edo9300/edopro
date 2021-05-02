#include "game_config.h"
#include <fstream>
#include <curl/curl.h>
#include <fmt/format.h>
#include "utils.h"
#include <IImage.h>
#include <IVideoDriver.h>
#include <IrrlichtDevice.h>
#include <IReadFile.h>
#include "logging.h"
#include "image_manager.h"
#include "image_downloader.h"
#include "game.h"

#define BASE_PATH EPRO_TEXT("./textures/")

namespace ygo {
////////kdiy/////
#define TEXTURE_DECK				0
#define TEXTURE_MENU				1
#define TEXTURE_COVERS				2
#define TEXTURE_COVERO				3
#define TEXTURE_ATTACK				4
#define TEXTURE_ACTIVATE			5
#define TEXTURE_CHAIN			    6
#define TEXTURE_NEGATED			    7
#define TEXTURE_LP		            8
#define TEXTURE_LPf		            9
#define TEXTURE_MASK		        10
#define TEXTURE_EQUIP		        11
#define TEXTURE_TARGET		        12
#define TEXTURE_CHAINTARGET		    13
#define TEXTURE_F1		            14
#define TEXTURE_F2		            15
#define TEXTURE_F3		            16
#define TEXTURE_BACKGROUND		    17
#define TEXTURE_BACKGROUND_MENU		18
#define TEXTURE_BACKGROUND_DECK		19
#define TEXTURE_field2		        20
#define TEXTURE_field_transparent2	21
#define TEXTURE_field3		        22
#define TEXTURE_field_transparent3	23
#define TEXTURE_field		        24
#define TEXTURE_field_transparent	25
#define TEXTURE_field4		        26
#define TEXTURE_field_transparent4	27
#define TEXTURE_field_fieldSP2	    28
#define TEXTURE_field_transparentSP2 29
#define TEXTURE_fieldSP3            30
#define TEXTURE_field_transparentSP3 31
#define TEXTURE_fieldSP             32
#define TEXTURE_field_transparentSP 33
#define TEXTURE_fieldSP4            34
#define TEXTURE_field_transparentSP4 35
#define TEXTURE_UNKNOWN             36
////////kdiy/////

#define X(x) (textures_path + EPRO_TEXT(x)).data()
#define GET(obj,fun1,fun2) do {obj=fun1; if(!obj) obj=fun2; def_##obj=obj;}while(0)
#define GTFF(path,ext,w,h) GetTextureFromFile(X(path ext), mainGame->Scale(w), mainGame->Scale(h))
#define GET_TEXTURE_SIZED(obj,path,w,h) GET(obj,GTFF(path,".png",w,h),GTFF(path,".jpg",w,h))
#define GET_TEXTURE(obj,path) GET(obj,driver->getTexture(X(path ".png")),driver->getTexture(X(path ".jpg")))
#define CHECK_RETURN(what, name) do { if(!what) { throw std::runtime_error("Couldn't load texture: " name); }} while(0)
ImageManager::ImageManager() {
	stop_threads = false;
	obj_clear_thread = std::thread(&ImageManager::ClearFutureObjects, this);
}
ImageManager::~ImageManager() {
	obj_clear_lock.lock();
	stop_threads = true;
	cv.notify_all();
	obj_clear_lock.unlock();
	obj_clear_thread.join();
	for(auto& it : g_imgCache) {
		if(it.second)
			it.second->drop();
	}
	for(auto& it : g_txrCache) {
		if(it.second)
			driver->removeTexture(it.second);
	}
}
bool ImageManager::Initial() {
	/////kdiy/////
	RefreshRandomImageList();
	/////kdiy/////	
	timestamp_id = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	textures_path = BASE_PATH;
	/////kdiy//////
	icon[0] = driver->getTexture(0);
	icon[1] = driver->getTexture(EPRO_TEXT("./textures/character/player/mini_icon.png"));
	icon[2] = driver->getTexture(EPRO_TEXT("./textures/character/atem/mini_icon.png"));
	icon[3] = driver->getTexture(EPRO_TEXT("./textures/character/kaiba/mini_icon.png"));
	icon[4] = driver->getTexture(EPRO_TEXT("./textures/character/joey/mini_icon.png"));
	icon[5] = driver->getTexture(EPRO_TEXT("./textures/character/DonThousand/mini_icon.png"));
	icon[6] = driver->getTexture(EPRO_TEXT("./textures/character/shark/mini_icon.png"));
	icon[7] = driver->getTexture(EPRO_TEXT("./textures/character/yuma/mini_icon.png"));
	character[0] = driver->getTexture(0);
	scharacter[0] = driver->getTexture(0);
	scharacter[1] = driver->getTexture(0);
	scharacter[2] = driver->getTexture(0);
	scharacter[3] = driver->getTexture(0);
	scharacter[4] = driver->getTexture(0);
	scharacter[5] = driver->getTexture(0);
	if(gGameConfig->character) {
	character[1] = driver->getTexture(EPRO_TEXT("./textures/character/player/icon.png"));
	character[2] = driver->getTexture(EPRO_TEXT("./textures/character/atem/icon.png"));
	character[3] = driver->getTexture(EPRO_TEXT("./textures/character/kaiba/icon.png"));
	character[4] = driver->getTexture(EPRO_TEXT("./textures/character/joey/icon.png"));
	character[5] = driver->getTexture(EPRO_TEXT("./textures/character/DonThousand/icon.png"));
	character[6] = driver->getTexture(EPRO_TEXT("./textures/character/shark/icon.png"));
	character[7] = driver->getTexture(EPRO_TEXT("./textures/character/yuma/icon.png"));
	} else {
	character[1] = driver->getTexture(0);
	character[2] = driver->getTexture(0);
	character[3] = driver->getTexture(0);
	character[4] = driver->getTexture(0);
	character[5] = driver->getTexture(0);
	character[6] = driver->getTexture(0);
	character[7] = driver->getTexture(0);
	}
	tcharacterselect = driver->getTexture(EPRO_TEXT("./textures/character/left.png"));
	tcharacterselect2 = driver->getTexture(EPRO_TEXT("./textures/character/right.png"));
	tCover[0] = GetRandomImage(TEXTURE_COVERS, CARD_IMG_WIDTH, CARD_IMG_HEIGHT);
	tCover[1] = GetRandomImage(TEXTURE_COVERO, CARD_IMG_WIDTH, CARD_IMG_HEIGHT);
	if (!tCover[0])
	    GET_TEXTURE_SIZED(tCover[0], "cover", CARD_IMG_WIDTH, CARD_IMG_HEIGHT);
	CHECK_RETURN(tCover[0], "cover");
	if (!tCover[1])
	    GET_TEXTURE_SIZED(tCover[1], "cover2", CARD_IMG_WIDTH, CARD_IMG_HEIGHT);
	if(!tCover[1]){
		tCover[1] = tCover[0];
		def_tCover[1] = tCover[1];
	}
	tUnknown = GetRandomImage(TEXTURE_UNKNOWN, CARD_IMG_WIDTH, CARD_IMG_HEIGHT);
	if (!tUnknown)
		GET_TEXTURE_SIZED(tUnknown, "unknown", CARD_IMG_WIDTH, CARD_IMG_HEIGHT);
	CHECK_RETURN(tUnknown, "unknown");	
	tAct = GetRandomImage(TEXTURE_ACTIVATE);
	tAttack = GetRandomImage(TEXTURE_ATTACK);
	if (!tAct)
		GET_TEXTURE(tAct, "act");
	CHECK_RETURN(tAct, "act");	
	if (!tAttack)
		GET_TEXTURE(tAttack, "attack");
	CHECK_RETURN(tAttack, "attack");	
	tChain = GetRandomImage(TEXTURE_CHAIN);
	if (!tChain)
		GET_TEXTURE(tChain, "chain");
	CHECK_RETURN(tChain, "chain");	
	tNegated = GetRandomImage(TEXTURE_NEGATED, 128, 128);
	if (!tNegated)
		GET_TEXTURE_SIZED(tNegated, "negated", 128, 128);
	CHECK_RETURN(tNegated, "negated");	
	GET_TEXTURE_SIZED(tNumber, "number", 320, 256);
	CHECK_RETURN(tNumber, "number");
	tLPBar = GetRandomImage(TEXTURE_LP);
	if (!tLPBar)
		GET_TEXTURE(tLPBar, "lp");
	CHECK_RETURN(tLPBar, "lp");
	tLPFrame = GetRandomImage(TEXTURE_LPf);
	if (!tLPFrame)
		GET_TEXTURE(tLPFrame, "lpf");
	CHECK_RETURN(tLPFrame, "lpf");	
	tMask = GetRandomImage(TEXTURE_MASK, 254, 254);
	if (!tMask)
		GET_TEXTURE_SIZED(tMask, "mask", 254, 254);
	CHECK_RETURN(tMask, "mask");	
	tEquip = GetRandomImage(TEXTURE_EQUIP);
	if (!tEquip)
		GET_TEXTURE(tEquip, "equip");
	CHECK_RETURN(tEquip, "equip");	
	tTarget = GetRandomImage(TEXTURE_TARGET);
	if (!tTarget)
		GET_TEXTURE(tTarget, "target");
	CHECK_RETURN(tTarget, "target");	
	tChainTarget = GetRandomImage(TEXTURE_CHAINTARGET);
	if (!tChainTarget)
		GET_TEXTURE(tChainTarget, "chaintarget");
	CHECK_RETURN(tChainTarget, "chaintarget");
	GET_TEXTURE(tLim, "lim");
	CHECK_RETURN(tLim, "lim");
	GET_TEXTURE(tOT, "ot");
	CHECK_RETURN(tOT, "ot");
	tHand[0] = GetRandomImage(TEXTURE_F1, 89, 128);
	if (!tHand[0])
		GET_TEXTURE_SIZED(tHand[0], "f1", 89, 128);
	CHECK_RETURN(tHand[0], "f1");
	tHand[1] = GetRandomImage(TEXTURE_F2, 89, 128);
	if (!tHand[1])
		GET_TEXTURE_SIZED(tHand[1], "f2", 89, 128);
	CHECK_RETURN(tHand[1], "f2");
	tHand[2] = GetRandomImage(TEXTURE_F3, 89, 128);
	if (!tHand[2])
		GET_TEXTURE_SIZED(tHand[2], "f3", 89, 128);
	CHECK_RETURN(tHand[2], "f3");
	tBackGround = GetRandomImage(TEXTURE_BACKGROUND);
	if (!tBackGround)
		GET_TEXTURE(tBackGround, "bg");
	CHECK_RETURN(tBackGround, "bg");
	tBackGround_menu = GetRandomImage(TEXTURE_BACKGROUND_MENU);
	if (!tBackGround_menu)
		GET_TEXTURE(tBackGround_menu, "bg_menu");
	if(!tBackGround_menu){
		tBackGround_menu = tBackGround;
		def_tBackGround_menu = tBackGround;
	}
	tBackGround_deck = GetRandomImage(TEXTURE_BACKGROUND_DECK);
	if (!tBackGround_deck)
		GET_TEXTURE(tBackGround_deck, "bg_deck");
	if(!tBackGround_deck){
		tBackGround_deck = tBackGround;
		def_tBackGround_deck = tBackGround;
	}
	tField[0][0] = GetRandomImage(TEXTURE_field2);
	if (!tField[0][0])
		GET_TEXTURE(tField[0][0], "field2");
	CHECK_RETURN(tField[0][0], "field2");
	tFieldTransparent[0][0] = GetRandomImage(TEXTURE_field_transparent2);
	if (!tFieldTransparent[0][0])
		GET_TEXTURE(tFieldTransparent[0][0], "field-transparent2");
	CHECK_RETURN(tFieldTransparent[0][0], "field-transparent2");
	tField[0][1] = GetRandomImage(TEXTURE_field3);
	if (!tField[0][1])
		GET_TEXTURE(tField[0][1], "field3");
	CHECK_RETURN(tField[0][1], "field3");
	tFieldTransparent[0][1] = GetRandomImage(TEXTURE_field_transparent3);
	if (!tFieldTransparent[0][1])
		GET_TEXTURE(tFieldTransparent[0][1], "field-transparent3");
	CHECK_RETURN(tFieldTransparent[0][1], "field-transparent3");
	tField[0][2] = GetRandomImage(TEXTURE_field);
	if (!tField[0][2])
		GET_TEXTURE(tField[0][2], "field");
	CHECK_RETURN(tField[0][2], "field");
	tFieldTransparent[0][2] = GetRandomImage(TEXTURE_field_transparent);
	if (!tFieldTransparent[0][2])
		GET_TEXTURE(tFieldTransparent[0][2], "field-transparent");
	CHECK_RETURN(tFieldTransparent[0][2], "field-transparent");
	tField[0][3] = GetRandomImage(TEXTURE_field4);
	if (!tField[0][3])
		GET_TEXTURE(tField[0][3], "field4");
	CHECK_RETURN(tField[0][3], "field4");
	tFieldTransparent[0][3] = GetRandomImage(TEXTURE_field_transparent4);
	if (!tFieldTransparent[0][3])
		GET_TEXTURE(tFieldTransparent[0][3], "field-transparent4");
	CHECK_RETURN(tFieldTransparent[0][3], "field-transparent4");
	tField[1][0] = GetRandomImage(TEXTURE_field_fieldSP2);
	if (!tField[1][0])
		GET_TEXTURE(tField[1][0], "fieldSP2");
	CHECK_RETURN(tField[1][0], "fieldSP2");
	tFieldTransparent[1][0] = GetRandomImage(TEXTURE_field_transparentSP2);
	if (!tFieldTransparent[1][0])
		GET_TEXTURE(tFieldTransparent[1][0], "field-transparentSP2");
	CHECK_RETURN(tFieldTransparent[1][0], "field-transparentSP2");
	tField[1][1] = GetRandomImage(TEXTURE_fieldSP3);
	if (!tField[1][1])
		GET_TEXTURE(tField[1][1], "fieldSP3");
	CHECK_RETURN(tField[1][1], "fieldSP3");
	tFieldTransparent[1][1] = GetRandomImage(TEXTURE_field_transparentSP3);
	if (!tFieldTransparent[1][1])
		GET_TEXTURE(tFieldTransparent[1][1], "field-transparentSP3");
	CHECK_RETURN(tFieldTransparent[1][1], "field-transparentSP3");
	tField[1][2] = GetRandomImage(TEXTURE_fieldSP);
	if (!tField[1][2])
		GET_TEXTURE(tField[1][2], "fieldSP");
	CHECK_RETURN(tField[1][2], "fieldSP");
	tFieldTransparent[1][2] = GetRandomImage(TEXTURE_field_transparentSP);
	if (!tFieldTransparent[1][2])
		GET_TEXTURE(tFieldTransparent[1][2], "field-transparentSP");
	CHECK_RETURN(tFieldTransparent[1][2], "field-transparentSP");
	tField[1][3] = GetRandomImage(TEXTURE_fieldSP4);
	if (!tField[1][3])
		GET_TEXTURE(tField[1][3], "fieldSP4");
	CHECK_RETURN(tField[1][3], "fieldSP4");
	tFieldTransparent[1][3] = GetRandomImage(TEXTURE_field_transparentSP4);
	if (!tFieldTransparent[1][3])
		GET_TEXTURE(tFieldTransparent[1][3], "field-transparentSP4");
	CHECK_RETURN(tFieldTransparent[1][3], "field-transparentSP4");
	char buff[100];
	for (int i = 0; i < 14; i++) {
		snprintf(buff, 100, "textures/pscale/rscale_%d.png", i);
		tRScale[i] = driver->getTexture(buff);
	}
	for (int i = 0; i < 14; i++) {
		snprintf(buff, 100, "textures/pscale/lscale_%d.png", i);
		tLScale[i] = driver->getTexture(buff);
	}
	///kdiy/////	
	GET_TEXTURE(tSettings, "settings");
	CHECK_RETURN(tSettings, "settings");
	sizes[0].first = sizes[1].first = CARD_IMG_WIDTH * gGameConfig->dpi_scale;
	sizes[0].second = sizes[1].second = CARD_IMG_HEIGHT * gGameConfig->dpi_scale;
	sizes[2].first = CARD_THUMB_WIDTH * gGameConfig->dpi_scale;
	sizes[2].second = CARD_THUMB_HEIGHT * gGameConfig->dpi_scale;
	return true;
}
//////kdiy//////
void ImageManager::SetAvatar(int seq, const wchar_t *avatar) {
	auto string = EPRO_TEXT("./textures/character/custom/") + Utils::ToPathString(avatar) + EPRO_TEXT(".png");
	scharacter[seq] = driver->getTexture(string.c_str());
}
void ImageManager::RefreshRandomImageList() {
	RefreshImageDir(EPRO_TEXT("bg_deck"), TEXTURE_DECK);
	RefreshImageDir(EPRO_TEXT("bg_menu"), TEXTURE_MENU);
	RefreshImageDir(EPRO_TEXT("cover"), TEXTURE_COVERS);
	RefreshImageDir(EPRO_TEXT("cover2"), TEXTURE_COVERO);
	RefreshImageDir(EPRO_TEXT("attack"), TEXTURE_ATTACK);
	RefreshImageDir(EPRO_TEXT("act"), TEXTURE_ACTIVATE);
	RefreshImageDir(EPRO_TEXT("chain"), TEXTURE_CHAIN);
	RefreshImageDir(EPRO_TEXT("negated"), TEXTURE_NEGATED);
	RefreshImageDir(EPRO_TEXT("lp"), TEXTURE_LP);
	RefreshImageDir(EPRO_TEXT("lpf"), TEXTURE_LPf);
	RefreshImageDir(EPRO_TEXT("mask"), TEXTURE_MASK);
	RefreshImageDir(EPRO_TEXT("equip"), TEXTURE_EQUIP);
	RefreshImageDir(EPRO_TEXT("target"), TEXTURE_TARGET);
	RefreshImageDir(EPRO_TEXT("chaintarget"), TEXTURE_CHAINTARGET);
	RefreshImageDir(EPRO_TEXT("f1"), TEXTURE_F1);
	RefreshImageDir(EPRO_TEXT("f2"), TEXTURE_F2);
	RefreshImageDir(EPRO_TEXT("f3"), TEXTURE_F3);
	RefreshImageDir(EPRO_TEXT("bg"), TEXTURE_BACKGROUND);
	RefreshImageDir(EPRO_TEXT("bg_menu"), TEXTURE_BACKGROUND_MENU);
	RefreshImageDir(EPRO_TEXT("bg_deck"), TEXTURE_BACKGROUND_DECK);
	RefreshImageDir(EPRO_TEXT("field2"), TEXTURE_field2);
	RefreshImageDir(EPRO_TEXT("field-transparent2"), TEXTURE_field_transparent2);
	RefreshImageDir(EPRO_TEXT("field3"), TEXTURE_field3);
	RefreshImageDir(EPRO_TEXT("field-transparent3"), TEXTURE_field_transparent3);
	RefreshImageDir(EPRO_TEXT("field"), TEXTURE_field);
	RefreshImageDir(EPRO_TEXT("field-transparent"), TEXTURE_field_transparent);
	RefreshImageDir(EPRO_TEXT("field4"), TEXTURE_field4);
	RefreshImageDir(EPRO_TEXT("field-transparent4"), TEXTURE_field_transparent4);
	RefreshImageDir(EPRO_TEXT("field-fieldSP2"), TEXTURE_field_fieldSP2);
	RefreshImageDir(EPRO_TEXT("field-transparentSP2"), TEXTURE_field_transparentSP2);
	RefreshImageDir(EPRO_TEXT("fieldSP3"), TEXTURE_fieldSP3);
	RefreshImageDir(EPRO_TEXT("field-transparentSP3"), TEXTURE_field_transparentSP3);
	RefreshImageDir(EPRO_TEXT("field-transparentSP"), TEXTURE_field_transparentSP);
	RefreshImageDir(EPRO_TEXT("fieldSP4"), TEXTURE_fieldSP4);
	RefreshImageDir(EPRO_TEXT("field-transparentSP4"), TEXTURE_field_transparentSP4);
	RefreshImageDir(EPRO_TEXT("unknown"), TEXTURE_UNKNOWN);

	for (int i = 0; i < 37; ++i) {
		saved_image_id[i] = -1;
	}
}
void ImageManager::RefreshImageDir(epro::path_string path, int image_type) {
	for (auto file : Utils::FindFiles(BASE_PATH + path, { EPRO_TEXT("jpg"), EPRO_TEXT("png") })) {
		auto folder = BASE_PATH + path + EPRO_TEXT("/") + file;
		ImageList[image_type].push_back(Utils::ToPathString(folder));
	}
}
irr::video::ITexture* ImageManager::GetRandomImage(int image_type) {
	int count = ImageList[image_type].size();
	if (count <= 0)
		return NULL;
	char ImageName[1024];
	wchar_t fname[1024];
	if (saved_image_id[image_type] == -1)
		saved_image_id[image_type] = rand() % count;
	int image_id = saved_image_id[image_type];
	auto name = ImageList[image_type][image_id].c_str();
	//BufferIO::EncodeUTF8(name, ImageName);
	return driver->getTexture(name);
}
irr::video::ITexture* ImageManager::GetRandomImage(int image_type, int width, int height) {
	int count = ImageList[image_type].size();
	if (count <= 0)
		return NULL;
	char ImageName[1024];
	wchar_t fname[1024];
	if (saved_image_id[image_type] == -1)
		saved_image_id[image_type] = rand() % count;
	int image_id = saved_image_id[image_type];
	auto name = ImageList[image_type][image_id].c_str();
	//BufferIO::EncodeUTF8(name, ImageName);
	return GetTextureFromFile(name, width, height);
}
//////kdiy//////

#undef GET
#undef GET_TEXTURE
#undef GET_TEXTURE_SIZED
#define GET(to_set,fun1,fun2,fallback) do  {\
	irr::video::ITexture* tmp = fun1;\
	if(!tmp)\
		tmp = fun2;\
	if(!tmp)\
		tmp = fallback;\
	if(to_set != fallback)\
		driver->removeTexture(to_set);\
	to_set = tmp;\
} while(0)
#define GET_TEXTURE_SIZED(obj,path,w,h) GET(obj,GTFF(path,".png",w,h),GTFF(path,".jpg",w,h),def_##obj)
#define GET_TEXTURE(obj,path) GET(obj,driver->getTexture(X(path ".png")),driver->getTexture(X(path ".jpg")),def_##obj)
void ImageManager::ChangeTextures(epro::path_stringview _path) {
	if(_path == textures_path)
		return;
	textures_path = { _path.data(), _path.size() };
	const bool is_base = textures_path == BASE_PATH;
	/////kdiy//////
	tAct = GetRandomImage(TEXTURE_ACTIVATE);
	tAttack = GetRandomImage(TEXTURE_ATTACK);
	if (!tAct)
		GET_TEXTURE(tAct, "act");
	if (!tAttack)
		GET_TEXTURE(tAttack, "attack");
	tChain = GetRandomImage(TEXTURE_CHAIN);
	if (!tChain)
		GET_TEXTURE(tChain, "chain");
	tNegated = GetRandomImage(TEXTURE_NEGATED, 128, 128);
	if (!tNegated)
		GET_TEXTURE_SIZED(tNegated, "negated", 128, 128);
	GET_TEXTURE_SIZED(tNumber, "number", 320, 256);
	tLPBar = GetRandomImage(TEXTURE_LP);
	if (!tLPBar)
		GET_TEXTURE(tLPBar, "lp");
	tLPFrame = GetRandomImage(TEXTURE_LPf);
	if (!tLPFrame)
		GET_TEXTURE(tLPFrame, "lpf");
	tMask = GetRandomImage(TEXTURE_MASK, 254, 254);
	if (!tMask)
		GET_TEXTURE_SIZED(tMask, "mask", 254, 254);
	tEquip = GetRandomImage(TEXTURE_EQUIP);
	if (!tEquip)
		GET_TEXTURE(tEquip, "equip");
	tTarget = GetRandomImage(TEXTURE_TARGET);
	if (!tTarget)
		GET_TEXTURE(tTarget, "target");
	tChainTarget = GetRandomImage(TEXTURE_CHAINTARGET);
	if (!tChainTarget)
		GET_TEXTURE(tChainTarget, "chaintarget");
	GET_TEXTURE(tLim, "lim");
	GET_TEXTURE(tOT, "ot");
	tHand[0] = GetRandomImage(TEXTURE_F1, 89, 128);
	if (!tHand[0])
		GET_TEXTURE_SIZED(tHand[0], "f1", 89, 128);
	tHand[1] = GetRandomImage(TEXTURE_F2, 89, 128);
	if (!tHand[1])
		GET_TEXTURE_SIZED(tHand[1], "f2", 89, 128);
	tHand[2] = GetRandomImage(TEXTURE_F3, 89, 128);
	if (!tHand[2])
		GET_TEXTURE_SIZED(tHand[2], "f3", 89, 128);
	tBackGround = GetRandomImage(TEXTURE_BACKGROUND);
	if (!tBackGround)
		GET_TEXTURE(tBackGround, "bg");
	tBackGround_menu = GetRandomImage(TEXTURE_BACKGROUND_MENU);
	if (!tBackGround_menu)
		GET_TEXTURE(tBackGround_menu, "bg_menu");
	if(!is_base && tBackGround != def_tBackGround && tBackGround_menu == def_tBackGround_menu)
		tBackGround_menu = tBackGround;
	tBackGround_deck = GetRandomImage(TEXTURE_BACKGROUND_DECK);
	if (!tBackGround_deck)
		GET_TEXTURE(tBackGround_deck, "bg_deck");
	if(!is_base && tBackGround != def_tBackGround && tBackGround_deck == def_tBackGround_deck)
		tBackGround_deck = tBackGround;
	tField[0][0] = GetRandomImage(TEXTURE_field2);
	if (!tField[0][0])
		GET_TEXTURE(tField[0][0], "field2");
	tFieldTransparent[0][0] = GetRandomImage(TEXTURE_field_transparent2);
	if (!tFieldTransparent[0][0])
		GET_TEXTURE(tFieldTransparent[0][0], "field-transparent2");
	tField[0][1] = GetRandomImage(TEXTURE_field3);
	if (!tField[0][1])
		GET_TEXTURE(tField[0][1], "field3");
	tFieldTransparent[0][1] = GetRandomImage(TEXTURE_field_transparent3);
	if (!tFieldTransparent[0][1])
		GET_TEXTURE(tFieldTransparent[0][1], "field-transparent3");
	tField[0][2] = GetRandomImage(TEXTURE_field);
	if (!tField[0][2])
		GET_TEXTURE(tField[0][2], "field");
	tFieldTransparent[0][2] = GetRandomImage(TEXTURE_field_transparent);
	if (!tFieldTransparent[0][2])
		GET_TEXTURE(tFieldTransparent[0][2], "field-transparent");
	tField[0][3] = GetRandomImage(TEXTURE_field4);
	if (!tField[0][3])
		GET_TEXTURE(tField[0][3], "field4");
	tFieldTransparent[0][3] = GetRandomImage(TEXTURE_field_transparent4);
	if (!tFieldTransparent[0][3])
		GET_TEXTURE(tFieldTransparent[0][3], "field-transparent4");
	tField[1][0] = GetRandomImage(TEXTURE_field_fieldSP2);
	if (!tField[1][0])
		GET_TEXTURE(tField[1][0], "fieldSP2");
	tFieldTransparent[1][0] = GetRandomImage(TEXTURE_field_transparentSP2);
	if (!tFieldTransparent[1][0])
		GET_TEXTURE(tFieldTransparent[1][0], "field-transparentSP2");
	tField[1][1] = GetRandomImage(TEXTURE_fieldSP3);
	if (!tField[1][1])
		GET_TEXTURE(tField[1][1], "fieldSP3");
	tFieldTransparent[1][1] = GetRandomImage(TEXTURE_field_transparentSP3);
	if (!tFieldTransparent[1][1])
		GET_TEXTURE(tFieldTransparent[1][1], "field-transparentSP3");
	tField[1][2] = GetRandomImage(TEXTURE_fieldSP);
	if (!tField[1][2])
		GET_TEXTURE(tField[1][2], "fieldSP");
	tFieldTransparent[1][2] = GetRandomImage(TEXTURE_field_transparentSP);
	if (!tFieldTransparent[1][2])
		GET_TEXTURE(tFieldTransparent[1][2], "field-transparentSP");
	tField[1][3] = GetRandomImage(TEXTURE_fieldSP4);
	if (!tField[1][3])
		GET_TEXTURE(tField[1][3], "fieldSP4");
	tFieldTransparent[1][3] = GetRandomImage(TEXTURE_field_transparentSP4);
	if (!tFieldTransparent[1][3])
		GET_TEXTURE(tFieldTransparent[1][3], "field-transparentSP4");
	char buff[100];
	for (int i = 0; i < 14; i++) {
		snprintf(buff, 100, "textures/pscale/rscale_%d.png", i);
		tRScale[i] = driver->getTexture(buff);
	}
	for (int i = 0; i < 14; i++) {
		snprintf(buff, 100, "textures/pscale/lscale_%d.png", i);
		tLScale[i] = driver->getTexture(buff);
	}
	/////kdiy//////
	GET_TEXTURE(tSettings, "settings");
	RefreshCovers();
}
void ImageManager::ResetTextures() {
	ChangeTextures(BASE_PATH);
}
void ImageManager::SetDevice(irr::IrrlichtDevice* dev) {
	device = dev;
	driver = dev->getVideoDriver();
}
void ImageManager::ClearTexture(bool resize) {
	auto ClearMap = [&](texture_map &map) {
		for(const auto& tit : map) {
			if(tit.second) {
				driver->removeTexture(tit.second);
			}
		}
		map.clear();
	};
	if(resize) {
		sizes[1].first = CARD_IMG_WIDTH * mainGame->window_scale.X * gGameConfig->dpi_scale;
		sizes[1].second = CARD_IMG_HEIGHT * mainGame->window_scale.Y * gGameConfig->dpi_scale;
		sizes[2].first = CARD_THUMB_WIDTH * mainGame->window_scale.X * gGameConfig->dpi_scale;
		sizes[2].second = CARD_THUMB_HEIGHT * mainGame->window_scale.Y * gGameConfig->dpi_scale;
		RefreshCovers();
	}
	if(!resize) {
		ClearCachedTextures(resize);
	}
	ClearMap(tMap[0]);
	ClearMap(tMap[1]);
	ClearMap(tThumb);
	ClearMap(tFields);
	ClearMap(tCovers);
}
#undef GET_TEXTURE
#undef GET_TEXTURE_SIZED
#undef X
void ImageManager::RemoveTexture(uint32_t code) {
	for(auto map : { &tMap[0], &tMap[1] }) {
		auto tit = map->find(code);
		if(tit != map->end()) {
			if(tit->second)
				driver->removeTexture(tit->second);
			map->erase(tit);
		}
	}
}
void ImageManager::RefreshCachedTextures() {
	auto StartLoad = [this](loading_map& src, texture_map& dest, int index, imgType type) {
		std::vector<int> readd;
		for (auto it = src.begin(); it != src.end();) {
			if (it->second.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
				auto pair = it->second.get();
				if (pair.first) {
					if (pair.first->getDimension().Width != sizes[index].first || pair.first->getDimension().Height != sizes[index].second) {
						readd.push_back(it->first);
						dest[it->first] = nullptr;
						it = src.erase(it);
						continue;
					}
					dest[it->first] = driver->addTexture(pair.second.data(), pair.first);
					pair.first->drop();
				} else if (pair.second != EPRO_TEXT("wait for download")) {
					dest[it->first] = nullptr;
				}
				it = src.erase(it);
				continue;
			}
			it++;
		}
		for (auto& code : readd) {
			src.emplace(code, std::async(std::launch::async, &ImageManager::LoadCardTexture, this, code, type, std::ref(sizes[index].first), std::ref(sizes[index].second), timestamp_id.load(), std::ref(timestamp_id)));
		}
	};
	StartLoad(loading_pics[0], tMap[0], 0, imgType::ART);
	StartLoad(loading_pics[1], tMap[1], 1, imgType::ART);
	StartLoad(loading_pics[2], tThumb, 2, imgType::THUMB);
	StartLoad(loading_pics[3], tCovers, 1, imgType::COVER);
}
void ImageManager::ClearFutureObjects() {
	Utils::SetThreadName("ImgObjsClear");
	while(!stop_threads) {
		std::unique_lock<std::mutex> lck(obj_clear_lock);
		while(to_clear.empty()) {
			cv.wait(lck);
			if(stop_threads)
				return;
		}
		auto img = std::move(to_clear.front());
		to_clear.pop_front();
		lck.unlock();
		auto pair = img.second.get();
		if(pair.first)
			pair.first->drop();
	}
}
void ImageManager::RefreshCovers() {
	irr::video::ITexture* tmp_cover = nullptr;
#undef GET
#define GET(obj,fun1,fun2) do {obj=fun1; if(!obj) obj=fun2;} while(0)
#define X(x) BASE_PATH x
#define GET_TEXTURE_SIZED(obj,path) do {GET(tmp_cover,GetTextureFromFile(X( path".png"),sizes[1].first,sizes[1].second),GetTextureFromFile(X( path".jpg"),sizes[1].first,sizes[1].second));\
										if(tmp_cover) {\
											driver->removeTexture(obj); \
											obj = tmp_cover;\
										}} while(0)
	/////kdiy//////
	tCover[0] = GetRandomImage(TEXTURE_COVERS);
	tCover[1] = GetRandomImage(TEXTURE_COVERO);
	if (!tCover[0])										
	GET_TEXTURE_SIZED(tCover[0], "cover");
	tCover[1] = nullptr;
	if (!tCover[1])	
	/////kdiy//////
	GET_TEXTURE_SIZED(tCover[1], "cover2");
	if(!tCover[1]) {
		tCover[1] = tCover[0];
		def_tCover[1] = tCover[1];
	}
	tUnknown = GetRandomImage(TEXTURE_UNKNOWN);
	if (!tUnknown)
	GET_TEXTURE_SIZED(tUnknown, "unknown");
#undef X
#define X(x) (textures_path + EPRO_TEXT(x)).data()
	if(textures_path != BASE_PATH) {
		GET(tmp_cover, GetTextureFromFile(X("cover.png"), sizes[1].first, sizes[1].second), GetTextureFromFile(X("cover.jpg"), sizes[1].first, sizes[1].second));
		if(tmp_cover){
			driver->removeTexture(tCover[0]);
			tCover[0] = tmp_cover;
		}
		GET(tmp_cover, GetTextureFromFile(X("cover2.png"), sizes[1].first, sizes[1].second), GetTextureFromFile(X("cover2.jpg"), sizes[1].first, sizes[1].second));
		if(tmp_cover){
			driver->removeTexture(tCover[1]);
			tCover[1] = tmp_cover;
		}
		GET(tmp_cover, GetTextureFromFile(X("unknown.png"), sizes[1].first, sizes[1].second), GetTextureFromFile(X("unknown.jpg"), sizes[1].first, sizes[1].second));
		if(tmp_cover){
			driver->removeTexture(tUnknown);
			tUnknown = tmp_cover;
		}
	}
#undef GET_TEXTURE_SIZED
#undef GET
#undef GTFF
}
void ImageManager::ClearCachedTextures(bool resize) {
	timestamp_id = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	std::unique_lock<std::mutex> lck(obj_clear_lock);
	for(auto& map : loading_pics) {
		to_clear.insert(to_clear.end(), std::make_move_iterator(map.begin()), std::make_move_iterator(map.end()));
		map.clear();
	}
	cv.notify_one();
}
// function by Warr1024, from https://github.com/minetest/minetest/issues/2419 , modified
bool ImageManager::imageScaleNNAA(irr::video::IImage* src, irr::video::IImage* dest, irr::s32 width, irr::s32 height, chrono_time timestamp_id, std::atomic<chrono_time>& source_timestamp_id) {
	// Cache rectsngle boundaries.
	const double sw = src->getDimension().Width;
	const double sh = src->getDimension().Height;

	// Walk each destination image pixel.
	// Note: loop y around x for better cache locality.
	const auto& dim = dest->getDimension();
	const auto divw = sw / dim.Width;
	const auto divh = sh / dim.Height;
	if(timestamp_id != source_timestamp_id.load())
		return false;
	for(irr::u32 dy = 0; dy < dim.Height; dy++)
		for(irr::u32 dx = 0; dx < dim.Width; dx++) {
			if(timestamp_id != source_timestamp_id.load())
				return false;

			// Calculate floating-point source rectangle bounds.
			const double minsx = dx * divw;
			const double maxsx = minsx + divw;
			const double minsy = dy * divh;
			const double maxsy = minsy + divh;

			// Total area, and integral of r, g, b values over that area,
			// initialized to zero, to be summed up in next loops.
			double area = 0, ra = 0, ga = 0, ba = 0, aa = 0;
			irr::video::SColor pxl;
			const auto csy = std::floor(minsy);
			const auto csx = std::floor(minsx);
			// Loop over the integral pixel positions described by those bounds.
			for(double sy = csy; sy < maxsy; sy++)
				for(double sx = csx; sx < maxsx; sx++) {
					if(timestamp_id != source_timestamp_id.load())
						return false;

					// Calculate width, height, then area of dest pixel
					// that's covered by this source pixel.
					double pw = 1;
					if(minsx > sx)
						pw += sx - minsx;
					if(maxsx < (sx + 1))
						pw += maxsx - sx - 1;
					double ph = 1;
					if(minsy > sy)
						ph += sy - minsy;
					if(maxsy < (sy + 1))
						ph += maxsy - sy - 1;
					const double pa = pw * ph;

					// Get source pixel and add it to totals, weighted
					// by covered area and alpha.
					pxl = src->getPixel(sx, sy);
					area += pa;
					ra += pa * pxl.getRed();
					ga += pa * pxl.getGreen();
					ba += pa * pxl.getBlue();
					aa += pa * pxl.getAlpha();
				}

			// Set the destination image pixel to the average color.
			if(area > 0) {
				pxl.setRed(ra / area + 0.5);
				pxl.setGreen(ga / area + 0.5);
				pxl.setBlue(ba / area + 0.5);
				pxl.setAlpha(aa / area + 0.5);
			} else {
				pxl.setRed(0);
				pxl.setGreen(0);
				pxl.setBlue(0);
				pxl.setAlpha(0);
			}
			dest->setPixel(dx, dy, pxl);
		}
	return true;
}
irr::video::IImage* ImageManager::GetScaledImage(irr::video::IImage* srcimg, int width, int height, chrono_time timestamp_id, std::atomic<chrono_time>& source_timestamp_id) {
	if(width <= 0 || height <= 0)
		return nullptr;
	if(!srcimg || timestamp_id != source_timestamp_id.load())
		return nullptr;
	const irr::core::dimension2d<irr::u32> dim(width, height);
	if(srcimg->getDimension() == dim) {
		srcimg->grab();
		return srcimg;
	} else {
		irr::video::IImage* destimg = driver->createImage(srcimg->getColorFormat(), dim);
		if(timestamp_id != source_timestamp_id.load() || !imageScaleNNAA(srcimg, destimg, width, height, timestamp_id, std::ref(source_timestamp_id))) {
			destimg->drop();
			destimg = nullptr;
		}
		return destimg;
	}
}
irr::video::ITexture* ImageManager::GetTextureFromFile(const irr::io::path& file, int width, int height) {
	auto img = GetScaledImageFromFile(file, width, height);
	if(img) {
		auto texture = driver->addTexture(file, img);
		img->drop();
		if(texture)
			return texture;
	}
	return driver->getTexture(file);
}
ImageManager::image_path ImageManager::LoadCardTexture(uint32_t code, imgType type, std::atomic<irr::s32>& _width, std::atomic<irr::s32>& _height, chrono_time timestamp_id, std::atomic<chrono_time>& source_timestamp_id) {
	static constexpr auto fail = std::make_pair(nullptr, EPRO_TEXT("fail"));
	static constexpr auto waitdownload = std::make_pair(nullptr, EPRO_TEXT("wait for download"));
	int width = _width;
	int height = _height;
	if(type == imgType::THUMB)
		type = imgType::ART;

	auto LoadImg = [&](irr::video::IImage* base_img)->irr::video::IImage* {
		if(!base_img)
			return nullptr;
		if(width != _width || height != _height) {
			width = _width;
			height = _height;
		}
		while(const auto img = GetScaledImage(base_img, width, height, timestamp_id, std::ref(source_timestamp_id))) {
			if(timestamp_id != source_timestamp_id.load()) {
				img->drop();
				base_img->drop();
				return nullptr;
			}
			if(width != _width || height != _height) {
				img->drop();
				width = _width;
				height = _height;
				continue;
			}
			base_img->drop();
			return img;
		}
		base_img->drop();
		return nullptr;
	};
	irr::video::IImage* img;
	auto status = gImageDownloader->GetDownloadStatus(code, type);
	if(status == ImageDownloader::downloadStatus::DOWNLOADED) {
		if(timestamp_id != source_timestamp_id.load())
			return fail;
		const auto file = gImageDownloader->GetDownloadPath(code, type);
		if((img = LoadImg(driver->createImageFromFile({ file.data(), (irr::u32)file.size() }))) != nullptr)
			return std::make_pair(img, file);
		return fail;
	} else if(status == ImageDownloader::downloadStatus::NONE) {
		for(auto& path : (type == imgType::ART) ? mainGame->pic_dirs : mainGame->cover_dirs) {
			for(auto extension : { EPRO_TEXT(".png"), EPRO_TEXT(".jpg") }) {
				if(timestamp_id != source_timestamp_id.load())
					return fail;
				irr::video::IImage* base_img = nullptr;
				epro::path_string file;
				if(path == EPRO_TEXT("archives")) {
					auto archiveFile = Utils::FindFileInArchives(
						(type == imgType::ART) ? EPRO_TEXT("pics/") : EPRO_TEXT("pics/cover/"),
						fmt::format(EPRO_TEXT("{}{}"), code, extension));
					if(!archiveFile)
						continue;
					const auto& name = archiveFile->getFileName();
					file = { name.c_str(), name.size() };
					base_img = driver->createImageFromFile(archiveFile);
					archiveFile->drop();
				} else {
					file = fmt::format(EPRO_TEXT("{}{}{}"), path, code, extension);
					base_img = driver->createImageFromFile({ file.data(), (irr::u32)file.size() });
				}
				if((img = LoadImg(base_img)) != nullptr)
					return std::make_pair(img, file);
			}
		}
		gImageDownloader->AddToDownloadQueue(code, type);
		return waitdownload;
	}
	return fail;
}
irr::video::ITexture* ImageManager::GetTextureCard(uint32_t code, imgType type, bool wait, bool fit, int* chk) {
	if(chk)
		*chk = 1;
	irr::video::ITexture* ret_unk = tUnknown;
	int index;
	int size_index;
	auto& map = [&]()->texture_map& {
		switch(type) {
			case imgType::ART: {
				index = fit ? 1 : 0;
				size_index = index;
				return tMap[fit ? 1 : 0];
			}
			case imgType::THUMB: {
				index = 2;
				size_index = index;
				return tThumb;
			}
			case imgType::COVER: {
				ret_unk = tCover[0];
				index = 3;
				size_index = 0;
				return tCovers;
			}
			// Should never come to these last cases
			case imgType::FIELD:
			default:
				return tMap[0];
		}
	}();
	if(code == 0)
		return ret_unk;
	auto tit = map.find(code);
	if(tit == map.end()) {
		auto status = gImageDownloader->GetDownloadStatus(code, type);
		if(status == ImageDownloader::downloadStatus::DOWNLOADING) {
			if(chk)
				*chk = 2;
			return ret_unk;
		}
		//pic will be loaded below instead
		/*if(status == ImageDownloader::DOWNLOADED) {
			map[code] = driver->getTexture(gImageDownloader->GetDownloadPath(code, type).data());
			return map[code] ? map[code] : ret_unk;
		}*/
		if(status == ImageDownloader::downloadStatus::DOWNLOAD_ERROR) {
			map[code] = nullptr;
			return ret_unk;
		}
		auto& load_entry = loading_pics[index];
		auto a = load_entry.find(code);
		if(chk)
			*chk = 2;
		if(a == load_entry.end()) {
			if(wait) {
				auto tmp_img = LoadCardTexture(code, type, std::ref(sizes[size_index].first), std::ref(sizes[size_index].second), timestamp_id.load(), std::ref(timestamp_id));
				auto& rmap = map[code];
				if(tmp_img.first) {
					rmap = driver->addTexture(tmp_img.second.data(), tmp_img.first);
					tmp_img.first->drop();
					if(chk)
						*chk = 1;
				} else {
					rmap = nullptr;
					if(chk)
						*chk = 0;
				}
				return (rmap) ? rmap : ret_unk;
			} else {
				load_entry.emplace(code, std::async(std::launch::async, &ImageManager::LoadCardTexture, this, code, type, std::ref(sizes[size_index].first), std::ref(sizes[size_index].second), timestamp_id.load(), std::ref(timestamp_id)));
			}
		}
		return ret_unk;
	}
	if(chk && !tit->second)
		*chk = 0;
	return (tit->second) ? tit->second : ret_unk;
}
irr::video::ITexture* ImageManager::GetTextureField(uint32_t code) {
	if(code == 0)
		return nullptr;
	auto tit = tFields.find(code);
	if(tit == tFields.end()) {
		auto status = gImageDownloader->GetDownloadStatus(code, imgType::FIELD);
		bool should_download = status == ImageDownloader::downloadStatus::NONE;
		irr::video::ITexture* img = nullptr;
		if(!should_download) {
			if(status == ImageDownloader::downloadStatus::DOWNLOADED) {
				const auto path = gImageDownloader->GetDownloadPath(code, imgType::FIELD);
				img = driver->getTexture({ path.data(), (irr::u32)path.size() });
			} else
				return nullptr;
		} else {
			for(auto& path : mainGame->field_dirs) {
				for(auto extension : { EPRO_TEXT(".png"), EPRO_TEXT(".jpg") }) {
					if(path == EPRO_TEXT("archives")) {
						auto archiveFile = Utils::FindFileInArchives(EPRO_TEXT("pics/field/"), fmt::format(EPRO_TEXT("{}{}"), code, extension));
						if (!archiveFile)
							continue;
						img = driver->getTexture(archiveFile);
						archiveFile->drop();
						if(img)
							break;
					} else {
						if((img = driver->getTexture(fmt::format(EPRO_TEXT("{}{}{}"), path, code, extension).data())))
							break;
					}
				}
			}
		}
		if(should_download && !img)
		    gImageDownloader->AddToDownloadQueue(code, imgType::FIELD);
		else
			tFields[code] = img;
		return img;
	}
	return (tit->second) ? tit->second : nullptr;
}


/*
From minetest: Copyright (C) 2015 Aaron Suen <warr1024@gmail.com>
https://github.com/minetest/minetest/blob/5506e97ed897dde2d4820fe1b021a4622bae03b3/src/client/guiscalingfilter.cpp
originally under LGPL2.1+
*/



/* Fill in RGB values for transparent pixels, to correct for odd colors
 * appearing at borders when blending.  This is because many PNG optimizers
 * like to discard RGB values of transparent pixels, but when blending then
 * with non-transparent neighbors, their RGB values will shpw up nonetheless.
 *
 * This function modifies the original image in-place.
 *
 * Parameter "threshold" is the alpha level below which pixels are considered
 * transparent.  Should be 127 for 3d where alpha is threshold, but 0 for
 * 2d where alpha is blended.
 */
static void imageCleanTransparent(irr::video::IImage* src, irr::u32 threshold) {
	const auto& dim = src->getDimension();

	// Walk each pixel looking for fully transparent ones.
	// Note: loop y around x for better cache locality.
	for(irr::u32 ctry = 0; ctry < dim.Height; ctry++)
		for(irr::u32 ctrx = 0; ctrx < dim.Width; ctrx++) {

			// Ignore opaque pixels.
			auto c = src->getPixel(ctrx, ctry);
			if(c.getAlpha() > threshold)
				continue;

			// Sample size and total weighted r, g, b values.
			irr::u32 ss = 0, sr = 0, sg = 0, sb = 0;

			// Walk each neighbor pixel (clipped to image bounds).
			for(irr::u32 sy = (ctry < 1) ? 0 : (ctry - 1);
				sy <= (ctry + 1) && sy < dim.Height; sy++)
				for(irr::u32 sx = (ctrx < 1) ? 0 : (ctrx - 1);
					sx <= (ctrx + 1) && sx < dim.Width; sx++) {

				// Ignore transparent pixels.
				const auto d = src->getPixel(sx, sy);
				if(d.getAlpha() <= threshold)
					continue;

				// Add RGB values weighted by alpha.
				const auto a = d.getAlpha();
				ss += a;
				sr += a * d.getRed();
				sg += a * d.getGreen();
				sb += a * d.getBlue();
			}

			// If we found any neighbor RGB data, set pixel to average
			// weighted by alpha.
			if(ss > 0) {
				c.setRed(sr / ss);
				c.setGreen(sg / ss);
				c.setBlue(sb / ss);
				src->setPixel(ctrx, ctry, c);
			}
		}
}

/* Scale a region of an image into another image, using nearest-neighbor with
 * anti-aliasing; treat pixels as crisp rectangles, but blend them at boundaries
 * to prevent non-integer scaling ratio artifacts.  Note that this may cause
 * some blending at the edges where pixels don't line up perfectly, but this
 * filter is designed to produce the most accurate results for both upscaling
 * and downscaling.
 */
static void imageScaleNNAAUnthreaded(irr::video::IImage* src, const irr::core::rect<irr::s32>& srcrect, irr::video::IImage* dest) {
	// Cache rectangle boundaries.
	const double sox = srcrect.UpperLeftCorner.X;
	const double soy = srcrect.UpperLeftCorner.Y;
	const double sw = srcrect.getWidth();
	const double sh = srcrect.getHeight();

	// Walk each destination image pixel.
	// Note: loop y around x for better cache locality.
	const auto& dim = dest->getDimension();
	const auto divw = sw / dim.Width;
	const auto divh = sh / dim.Height;
	for(irr::u32 dy = 0; dy < dim.Height; dy++)
		for(irr::u32 dx = 0; dx < dim.Width; dx++) {

			// Calculate floating-point source rectangle bounds.
			// Do some basic clipping, and for mirrored/flipped rects,
			// make sure min/max are in the right order.
			auto minsx = std::min(std::max(sox + (dx * divw), 0.0), sw + sox);
			auto maxsx = std::min(std::max(minsx + divw, 0.0), sw + sox);
			if(minsx > maxsx)
				std::swap(minsx, maxsx);
			auto minsy = std::min(std::max(soy + (dy * divh), 0.0), sh + soy);
			auto maxsy = std::min(std::max(minsy + divh, 0.0), sh + soy);
			if(minsy > maxsy)
				std::swap(minsy, maxsy);

			const auto csy = std::floor(minsy);
			const auto csx = std::floor(minsx);

			// Total area, and integral of r, g, b values over that area,
			// initialized to zero, to be summed up in next loops.
			double area = 0, ra = 0, ga = 0, ba = 0, aa = 0;
			irr::video::SColor pxl;

			// Loop over the integral pixel positions described by those bounds.
			for(double sy = csy; sy < maxsy; sy++)
				for(double sx = csx; sx < maxsx; sx++) {
					// Calculate width, height, then area of dest pixel
					// that's covered by this source pixel.

					double pw = 1.0;
					if(minsx > sx)
						pw += sx - minsx;
					if(maxsx < (sx + 1))
						pw += maxsx - sx - 1;
					double ph = 1.0;
					if(minsy > sy)
						ph += sy - minsy;
					if(maxsy < (sy + 1))
						ph += maxsy - sy - 1;
					const double pa = pw * ph;

					// Get source pixel and add it to totals, weighted
					// by covered area and alpha.
					pxl = src->getPixel(sx, sy);
					area += pa;
					ra += pa * pxl.getRed();
					ga += pa * pxl.getGreen();
					ba += pa * pxl.getBlue();
					aa += pa * pxl.getAlpha();
				}

			// Set the destination image pixel to the average color.
			if(area > 0) {
				pxl.setRed(ra / area + 0.5);
				pxl.setGreen(ga / area + 0.5);
				pxl.setBlue(ba / area + 0.5);
				pxl.setAlpha(aa / area + 0.5);
			} else {
				pxl.setRed(0);
				pxl.setGreen(0);
				pxl.setBlue(0);
				pxl.setAlpha(0);
			}
			dest->setPixel(dx, dy, pxl);
		}
}
#ifdef __ANDROID__
static bool hasNPotSupport(irr::video::IVideoDriver* driver) {
	static const auto supported = [driver] {
		return driver->queryFeature(irr::video::EVDF_TEXTURE_NPOT);
	}();
	return supported;
}
// Compute next-higher power of 2 efficiently, e.g. for power-of-2 texture sizes.
// Public Domain: https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static inline irr::u32 npot2(irr::u32 orig) {
	orig--;
	orig |= orig >> 1;
	orig |= orig >> 2;
	orig |= orig >> 4;
	orig |= orig >> 8;
	orig |= orig >> 16;
	return orig + 1;
}
#endif
/* Get a cached, high-quality pre-scaled texture for display purposes.  If the
 * texture is not already cached, attempt to create it.  Returns a pre-scaled texture,
 * or the original texture if unable to pre-scale it.
 */
irr::video::ITexture* ImageManager::guiScalingResizeCached(irr::video::ITexture* src, const irr::core::rect<irr::s32> &srcrect,
											const irr::core::rect<irr::s32> &destrect) {
	if(!src)
		return src;

	const auto& origname = src->getName().getPath();
	// Calculate scaled texture name.
	const auto scale_name = fmt::format(EPRO_TEXT("{}@guiScalingFilter:{}:{}:{}:{}:{}:{}"),
						 origname,
						 srcrect.UpperLeftCorner.X,
						 srcrect.UpperLeftCorner.Y,
						 srcrect.getWidth(),
						 srcrect.getHeight(),
						 destrect.getWidth(),
						 destrect.getHeight());
	irr::io::path scalename{ scale_name.data(), (irr::u32)scale_name.size() };

	// Search for existing scaled texture.
	irr::video::ITexture* scaled = g_txrCache[scalename];
	if(scaled)
		return scaled;

	// Try to find the texture converted to an image in the cache.
	// If the image was not found, try to extract it from the texture.
	irr::video::IImage* srcimg = g_imgCache[origname];
	if(!srcimg) {
		srcimg = driver->createImageFromData(src->getColorFormat(),
											 src->getSize(), src->lock(), false);
		src->unlock();
		g_imgCache[origname] = srcimg;
	}

	// Create a new destination image and scale the source into it.
	imageCleanTransparent(srcimg, 0);
	irr::video::IImage* destimg = driver->createImage(src->getColorFormat(),
													  irr::core::dimension2d<irr::u32>((irr::u32)destrect.getWidth(),
													 (irr::u32)destrect.getHeight()));
	imageScaleNNAAUnthreaded(srcimg, srcrect, destimg);

#ifdef __ANDROID__
	// Some platforms are picky about textures being powers of 2, so expand
	// the image dimensions to the next power of 2, if necessary.
	if(!hasNPotSupport(driver)) {
		irr::video::IImage *po2img = driver->createImage(src->getColorFormat(),
														 irr::core::dimension2d<irr::u32>(npot2((irr::u32)destrect.getWidth()),
																		   npot2((irr::u32)destrect.getHeight())));
		po2img->fill(irr::video::SColor(0, 0, 0, 0));
		destimg->copyTo(po2img);
		destimg->drop();
		destimg = po2img;
	}
#endif

	// Convert the scaled image back into a texture.
	scaled = driver->addTexture(scalename, destimg);
	destimg->drop();
	g_txrCache[scalename] = scaled;

	return scaled;
}
void ImageManager::draw2DImageFilterScaled(irr::video::ITexture* txr,
							 const irr::core::rect<irr::s32>& destrect, const irr::core::rect<irr::s32>& srcrect,
							 const irr::core::rect<irr::s32>* cliprect, const irr::video::SColor* const colors,
							 bool usealpha) {
	// Attempt to pre-scale image in software in high quality.
	irr::video::ITexture* scaled = guiScalingResizeCached(txr, srcrect, destrect);
	if(!scaled)
		return;

	// Correct source rect based on scaled image.
	const auto mysrcrect = (scaled != txr)
		? irr::core::rect<irr::s32>(0, 0, destrect.getWidth(), destrect.getHeight())
		: srcrect;

	driver->draw2DImage(scaled, destrect, mysrcrect, cliprect, colors, usealpha);
}
irr::video::IImage* ImageManager::GetScaledImageFromFile(const irr::io::path& file, int width, int height) {
	if(width <= 0 || height <= 0)
		return nullptr;

	auto* srcimg = driver->createImageFromFile(file);
	if(!srcimg)
		return nullptr;

	const irr::core::dimension2d<irr::u32> dim(width, height);
	const auto& srcdim = srcimg->getDimension();
	if(srcdim == dim) {
		return srcimg;
	} else {
		auto* destimg = driver->createImage(srcimg->getColorFormat(), dim);
		imageScaleNNAAUnthreaded(srcimg, { 0, 0, (irr::s32)srcdim.Width, (irr::s32)srcdim.Height }, destimg);
		srcimg->drop();
		return destimg;
	}
}

}
