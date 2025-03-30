/*
   CGUITTFont FreeType class for Irrlicht
   Copyright (c) 2009-2010 John Norman

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any
   purpose, including commercial applications, and to alter it and
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you
	  must not claim that you wrote the original software. If you use
	  this software in a product, an acknowledgment in the product
	  documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and
	  must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
	  distribution.

   The original version of this class can be located at:
   http://irrlicht.suckerfreegames.com/

   John Norman
   john@suckerfreegames.com
*/

#include "CGUITTFont.h"
#include FT_MODULE_H
#include <IVideoDriver.h>
#include <IrrlichtDevice.h>
#include <IGUIEnvironment.h>
#include <IFileSystem.h>
#include <SMeshBuffer.h>
#include <ISceneManager.h>
#include <IMeshManipulator.h>
#include <IMeshSceneNode.h>
#include <unordered_set>
#include <climits>
#include "../fmt.h"
#ifdef YGOPRO_USE_BUNDLED_FONT
extern const char* bundled_font_name;
extern const size_t bundled_font_len;
extern const unsigned char bundled_font[];
#endif

static_assert(FREETYPE_MAJOR == 2 && (FREETYPE_MINOR > 1 || (FREETYPE_MINOR == 1 && FREETYPE_PATCH >= 3)), "Freetype 2.1.3 or greater is required");

namespace irr {
namespace gui {

// Manages the FT_Face cache.
struct SGUITTFace final : public virtual irr::IReferenceCounted {
	SGUITTFace(FT_Face face) : face{ face } {}
	~SGUITTFace() {
		if(face)
			FT_Done_Face(face);
	}
	FT_Face face;
};

// Static variables.
FT_Library CGUITTFont::c_library = nullptr;
core::map<io::path, SGUITTFace*> CGUITTFont::c_faces;

//
#if IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9
#define GetData(image) image->getData()
#define Unlock(image)(void)0
#else
#define GetData(image) image->lock()
#define Unlock(image) image->unlock()
#endif

video::IImage* SGUITTGlyph::createGlyphImage(const FT_Bitmap& bits, video::IVideoDriver* driver) const {
	// Determine what our texture size should be.
	// Add 1 because textures are inclusive-exclusive.
	core::dimension2du d(bits.width + 1, bits.rows + 1);
	core::dimension2du texture_size;
	//core::dimension2du texture_size(bits.width + 1, bits.rows + 1);

	// Create and load our image now.
	video::IImage* image = nullptr;
	switch(bits.pixel_mode) {
		case FT_PIXEL_MODE_MONO: {
			// Create a blank image and fill it with transparent pixels.
			texture_size = d.getOptimalSize(true, true);
			image = driver->createImage(video::ECF_A1R5G5B5, texture_size);
			image->fill(video::SColor(0, 255, 255, 255));

			// Load the monochrome data in.
			const u32 image_pitch = image->getPitch() / sizeof(u16);
			u16* image_data = (u16*)GetData(image);
			u8* glyph_data = bits.buffer;
			for(u32 y = 0; y < bits.rows; ++y) {
				u16* row = image_data;
				for(u32 x = 0; x < bits.width; ++x) {
					// Monochrome bitmaps store 8 pixels per byte.  The left-most pixel is the bit 0x80.
					// So, we go through the data each bit at a time.
					if((glyph_data[y * bits.pitch + (x / 8)] & (0x80 >> (x % 8))) != 0)
						*row = 0xFFFF;
					++row;
				}
				image_data += image_pitch;
			}
			Unlock(image);
			break;
		}

		case FT_PIXEL_MODE_GRAY: {
			// Create our blank image.
			texture_size = d.getOptimalSize(!driver->queryFeature(video::EVDF_TEXTURE_NPOT), !driver->queryFeature(video::EVDF_TEXTURE_NSQUARE), true, 0);
			image = driver->createImage(video::ECF_A8R8G8B8, texture_size);
			image->fill(video::SColor(0, 255, 255, 255));

			// Load the grayscale data in.
			const float gray_count = static_cast<float>(bits.num_grays);
			const u32 image_pitch = image->getPitch() / sizeof(u32);
			u32* image_data = (u32*)GetData(image);
			u8* glyph_data = bits.buffer;
			for(u32 y = 0; y < bits.rows; ++y) {
				u8* row = glyph_data;
				for(u32 x = 0; x < bits.width; ++x) {
					image_data[y * image_pitch + x] |= static_cast<u32>(255.0f * (static_cast<float>(*row++) / gray_count)) << 24;
					//data[y * image_pitch + x] |= ((u32)(*bitsdata++) << 24);
				}
				glyph_data += bits.pitch;
			}
			Unlock(image);
			break;
		}
		default:
			// TODO: error message?
			return nullptr;
	}
	return image;
}

void SGUITTGlyph::preload(u32 char_index, FT_Face face, video::IVideoDriver* driver, u32 font_size, const FT_Int32 loadFlags) {
	if(isLoaded) return;

	// Set the size of the glyph.
	FT_Set_Pixel_Sizes(face, 0, font_size);

	// Attempt to load the glyph.
	if(FT_Load_Glyph(face, char_index, loadFlags) != FT_Err_Ok)
		// TODO: error message?
		return;

	FT_GlyphSlot glyph = face->glyph;
	FT_Bitmap bits = glyph->bitmap;

	// Setup the glyph information here:
	advance = glyph->advance;
	offset = core::vector2di(glyph->bitmap_left, glyph->bitmap_top);

	// Try to get the last page with available slots.
	CGUITTGlyphPage* page = parent->getLastGlyphPage();

	// If we need to make a new page, do that now.
	if(!page) {
		page = parent->createGlyphPage(bits.pixel_mode);
		if(!page)
			// TODO: add error message?
			return;
	}

	glyph_page = parent->getLastGlyphPageIndex();
	u32 texture_side_length = page->texture_size.Width - font_size;
	u32 margin = (u32)(font_size * 0.5);
	u32 sprite_size = (u32)(font_size * 1.5);
	core::vector2di page_position(
		(s32)(page->used_slots % (s32)(texture_side_length / sprite_size)) * sprite_size + margin,
		(s32)(page->used_slots / (s32)(texture_side_length / sprite_size)) * sprite_size + margin
	);
	source_rect.UpperLeftCorner = page_position;
	source_rect.LowerRightCorner = core::vector2di(page_position.X + bits.width, page_position.Y + bits.rows);

	page->dirty = true;
	++page->used_slots;
	--page->available_slots;

	// We grab the glyph bitmap here so the data won't be removed when the next glyph is loaded.
	surface = createGlyphImage(bits, driver);

	// Set our glyph as loaded.
	isLoaded = true;
}

void SGUITTGlyph::unload() {
	if(surface) {
		surface->drop();
		surface = nullptr;
	}
	isLoaded = false;
}

#if defined(TT_CONFIG_OPTION_SUBPIXEL_HINTING)
#define forceHinting(library) do { FT_UInt val = 35; FT_Property_Set(library, "truetype", "interpreter-version", &val); } while(0)
#else
#define forceHinting(library) (void)0
#endif

//////////////////////

CGUITTFont* CGUITTFont::createTTFont(IrrlichtDevice* device, IGUIEnvironment* env, const FontInfo& font_info, FallbackFonts::const_iterator fallback_begin, FallbackFonts::const_iterator fallback_end, const bool antialias, const bool transparency) {
	if(c_library == nullptr) {
		if(FT_Init_FreeType(&c_library))
			return nullptr;
		forceHinting(c_library);
	}

	CGUITTFont* font = new CGUITTFont(env);
	font->Device = device;
	bool ret = font->load(font_info.font, font_info.size, antialias, transparency);
	if(!ret) {
		font->drop();
		return nullptr;
	}

	if(fallback_begin != fallback_end) {
		const auto& fallback_font = *fallback_begin;
		font->fallback = createTTFont(device, env, fallback_font, ++fallback_begin, fallback_end, antialias, transparency);
		if(!font->fallback) {
			font->drop();
			return nullptr;
		}
	}

	return font;
}

//////////////////////

//! Constructor.
CGUITTFont::CGUITTFont(IGUIEnvironment *env)
	: use_monochrome(false), use_transparency(true), use_hinting(true), use_auto_hinting(true),
	batch_load_size(1), Device(nullptr), Environment(env), Driver(nullptr), GlobalKerningWidth(0), GlobalKerningHeight(0), supposed_line_height(0),
	fallback(nullptr) {
#ifdef _DEBUG
	setDebugName("CGUITTFont");
#endif

	if(Environment) {
		// don't grab environment, to avoid circular references
		Driver = Environment->getVideoDriver();
		Driver->grab();
	}

	setInvisibleCharacters(L" ");

	// Glyphs aren't reference counted, so don't try to delete them when we free the array.
	Glyphs.set_free_when_destroyed(false);
}

static unsigned long ReadFTStream(FT_Stream stream, unsigned long offset, unsigned char* buffer, unsigned long count) {
	auto* file = static_cast<io::IReadFile*>(stream->descriptor.pointer);
	if(stream->pos != offset && !file->seek(static_cast<long>(offset)))
		return count == 0 ? 1 : 0;
	size_t read = 0;
	if(count != 0)
		read = file->read(buffer, static_cast<size_t>(count));
	return static_cast<unsigned long>(read);
}

static void CloseFTStream(FT_Stream stream) {
	static_cast<io::IReadFile*>(stream->descriptor.pointer)->drop();
	delete stream;
}

static SGUITTFace* OpenFileStreamFont(FT_Library library, io::IReadFile* file) {
	FT_Open_Args args{};
	args.flags = FT_OPEN_STREAM;
	args.stream = new FT_StreamRec{};

	auto& stream = *args.stream;
	stream.size = static_cast<unsigned long>(file->getSize());
	stream.descriptor.pointer = file;
	stream.read = ReadFTStream;
	stream.close = CloseFTStream;
	FT_Face freetype_face = nullptr;
	if(FT_Open_Face(library, &args, 0, &freetype_face) != FT_Err_Ok)
		return nullptr;
	return new SGUITTFace{ freetype_face };
}

#ifdef YGOPRO_USE_BUNDLED_FONT
static SGUITTFace* OpenMemoryStreamFont(FT_Library library, const void* data, size_t size) {
	FT_Face freetype_face = nullptr;
	if(FT_New_Memory_Face(library, static_cast<const FT_Byte*>(data), static_cast<FT_Long>(size), 0, &freetype_face) != FT_Err_Ok)
		return nullptr;
	return new SGUITTFace{ freetype_face };
}
#endif

bool CGUITTFont::load(const io::path& font_filename, const u32 font_size, const bool antialias, const bool transparency) {
	// Some sanity checks.
	if(Environment == nullptr || Driver == nullptr) return false;
	if(font_size == 0) return false;
	if(font_filename.empty()) return false;

	io::IFileSystem* filesystem = Environment->getFileSystem();
	irr::ILogger* logger = (Device != 0 ? Device->getLogger() : 0);
	size = font_size;
	filename = font_filename;

	// Update the font loading flags when the font is first loaded.
	use_monochrome = !antialias;
	use_transparency = transparency;
	update_load_flags();

	// Log.
	if(logger)
		logger->log(L"CGUITTFont", core::stringw(core::stringw(L"Creating new font: ") + core::stringc(font_filename) + L" " + core::stringc(font_size) + L"pt " + (antialias ? L"+antialias " : L"-antialias ") + (transparency ? L"+transparency" : L"-transparency")).c_str(), irr::ELL_INFORMATION);

	// Grab the face.
	SGUITTFace* face = nullptr;
	auto* node = c_faces.find(font_filename);
	if(node == nullptr) {
#ifdef YGOPRO_USE_BUNDLED_FONT
		if(filename.equals_ignore_case(_IRR_TEXT("bundled")))
			face = OpenMemoryStreamFont(c_library, bundled_font, bundled_font_len);
		else
#endif //YGOPRO_USE_BUNDLED_FONT
		if(filesystem) {
			// Read in the file data.
			io::IReadFile* file = filesystem->createAndOpenFile(font_filename);
			if(file == nullptr) {
				if(logger) logger->log(L"CGUITTFont", L"Failed to open the file.", irr::ELL_INFORMATION);
				return false;
			}
			face = OpenFileStreamFont(c_library, file);
		}
		// Create the face.
		if(face == nullptr) {
			if(logger) logger->log(L"CGUITTFont", L"OpenFileStreamFont failed.", irr::ELL_INFORMATION);
			return false;
		}
		c_faces.set(font_filename, face);
	} else {
		// Using another instance of this face.
		face = node->getValue();
		face->grab();
	}

	// Store our face.
	tt_face = face->face;

	// Store font metrics.
	FT_Set_Pixel_Sizes(tt_face, font_size, 0);
	font_metrics = tt_face->size->metrics;

	// Allocate our glyphs.
	Glyphs.clear();
	Glyphs.reallocate(tt_face->num_glyphs);
	Glyphs.set_used(tt_face->num_glyphs);
	for(FT_Long i = 0; i < tt_face->num_glyphs; ++i) {
		Glyphs[i].isLoaded = false;
		Glyphs[i].glyph_page = 0;
		Glyphs[i].source_rect = core::recti();
		Glyphs[i].offset = core::vector2di();
		Glyphs[i].advance = FT_Vector();
		Glyphs[i].surface = nullptr;
		Glyphs[i].parent = this;
	}

	// Cache the first 127 ascii characters.
	u32 old_size = batch_load_size;
	batch_load_size = 127;
	getGlyphIndexByChar(uchar32_t(0), nullptr, nullptr);
	batch_load_size = old_size;

	// Calculate the supposed line height of this font (of this size) --
	// Not using FT_SizeMetric::ascender or height, but actually by testing some of the glyphs,
	// to see what should give a reasonable not cluttered line height.
	// The ascender or height info may as well just be arbitrary ones.

	// Get the maximum font height.  Unfortunately, we have to do this hack as
	// Irrlicht will draw things wrong.  In FreeType, the font size is the
	// maximum size for a single glyph, but that glyph may hang "under" the
	// draw line, increasing the total font height to beyond the set size.
	// Irrlicht does not understand this concept when drawing fonts.  Also, I
	// add +1 to give it a 1 pixel blank border.  This makes things like
	// tooltips look nicer.
	s32 test1 = getHeightFromCharacter(uchar32_t('g')) + 1;
	s32 test2 = getHeightFromCharacter(uchar32_t('j')) + 1;
	s32 test3 = getHeightFromCharacter(uchar32_t(0x55B5)) + 1;

	supposed_line_height = core::max_(test1, core::max_(test2, test3));

	return true;
}

CGUITTFont::~CGUITTFont() {
	if(fallback)
		fallback->drop();
	// Delete the glyphs and glyph pages.
	reset_images();
	CGUITTAssistDelete::Delete(Glyphs);
	//Glyphs.clear();

	// We aren't using this face anymore.
	core::map<io::path, SGUITTFace*>::Node* n = c_faces.find(filename);
	if(n) {
		SGUITTFace* f = n->getValue();

		// Drop our face.  If this was the last face, the destructor will clean up.
		if(f->drop())
			c_faces.remove(filename);

		// If there are no more faces referenced by FreeType, clean up.
		if(c_faces.empty()) {
			FT_Done_FreeType(c_library);
			c_library = nullptr;
		}
	}

	// Drop our driver now.
	if(Driver)
		Driver->drop();
}

void CGUITTFont::reset_images() {
	// Delete the glyphs.
	for(u32 i = 0; i != Glyphs.size(); ++i)
		Glyphs[i].unload();

	// Unload the glyph pages from video memory.
	for(u32 i = 0; i != Glyph_Pages.size(); ++i)
		delete Glyph_Pages[i];
	Glyph_Pages.clear();

	// Always update the internal FreeType loading flags after resetting.
	update_load_flags();
}

void CGUITTFont::update_glyph_pages() const {
	for(u32 i = 0; i != Glyph_Pages.size(); ++i) {
		if(Glyph_Pages[i]->dirty)
			Glyph_Pages[i]->updateTexture();
	}
	if(fallback)
		fallback->update_glyph_pages();
}

CGUITTGlyphPage* CGUITTFont::getLastGlyphPage() const {
	if(Glyph_Pages.empty())
		return nullptr;
	CGUITTGlyphPage* page = Glyph_Pages.getLast();
	if(page->available_slots == 0)
		page = nullptr;
	return page;
}

CGUITTGlyphPage* CGUITTFont::createGlyphPage(const u8& pixel_mode) {
	// Name of our page.
	auto name = epro::format("TTFontGlyphPage_{}.{}.{}._{}",
							tt_face->family_name, tt_face->style_name, size, Glyph_Pages.size()); // The newly created page will be at the end of the collection.

	// Create the new page.
	auto* page = new CGUITTGlyphPage(Driver, name);

	// Determine our maximum texture size.
	// If we keep getting 0, set it to 1024x1024, as that number is pretty safe.
	core::dimension2du max_texture_size = max_page_texture_size;
	if(max_texture_size.Width == 0 || max_texture_size.Height == 0)
		max_texture_size = Driver->getMaxTextureSize();
	if(max_texture_size.Width == 0 || max_texture_size.Height == 0)
		max_texture_size = core::dimension2du(1024, 1024);

	// We want to try to put at least 144 glyphs on a single texture.
	core::dimension2du page_texture_size;
	if(size <= 21) page_texture_size = core::dimension2du(256, 256);
	else if(size <= 42) page_texture_size = core::dimension2du(512, 512);
	else if(size <= 84) page_texture_size = core::dimension2du(1024, 1024);
	else if(size <= 168) page_texture_size = core::dimension2du(2048, 2048);
	else page_texture_size = core::dimension2du(4096, 4096);

	if(page_texture_size.Width > max_texture_size.Width || page_texture_size.Height > max_texture_size.Height)
		page_texture_size = max_texture_size;

	page->texture_size = page_texture_size;

	if(!page->createPageTexture(pixel_mode, page_texture_size)) {
		// TODO: add error message?
		delete page;
		return 0;
	}

	// Determine the number of glyph slots on the page and add it to the list of pages.
	page->available_slots = (u32)((page_texture_size.Width - size) / (u32)(size * 1.5)) * (u32)((page_texture_size.Height - size) / (u32)(size * 1.5));
	Glyph_Pages.push_back(page);
	return page;
}

void CGUITTFont::setTransparency(const bool flag) {
	use_transparency = flag;
	reset_images();
	if(fallback)
		fallback->setTransparency(flag);
}

void CGUITTFont::setMonochrome(const bool flag) {
	use_monochrome = flag;
	reset_images();
	if(fallback)
		fallback->setMonochrome(flag);
}

void CGUITTFont::setFontHinting(const bool enable, const bool enable_auto_hinting) {
	use_hinting = enable;
	use_auto_hinting = enable_auto_hinting;
	reset_images();
	if(fallback)
		fallback->setFontHinting(enable, enable_auto_hinting);
}

void CGUITTFont::draw(const core::stringw& text, const core::rect<s32>& position, video::SColor color, bool hcenter, bool vcenter, const core::rect<s32>* clip) {
	if(!Driver)
		return;
	drawustring(text, position, color, hcenter, vcenter, clip);
}

void CGUITTFont::drawustring(const core::ustring& utext, const core::rect<s32>& position, video::SColor color, bool hcenter, bool vcenter, const core::rect<s32>* clip) {
	if(!Driver)
		return;

	// Clear the glyph pages of their render information.
	clearGlyphPages();

	// Set up some variables.
	core::dimension2d<s32> textDimension;
	core::vector2d<s32> offset = position.UpperLeftCorner;

	// Determine offset positions.
	if(hcenter || vcenter) {
		textDimension = getDimensionustring(utext);

		if(hcenter)
			offset.X = ((position.getWidth() - textDimension.Width) >> 1) + offset.X;

		if(vcenter)
			offset.Y = ((position.getHeight() - textDimension.Height) >> 1) + offset.Y;
	}

	// Set up our render map.
	std::unordered_set<CGUITTGlyphPage*> Render_Map;

	// Start parsing characters.
	u32 n;
	uchar32_t previousChar = 0;
	auto iter = utext.begin();
	while(!iter.atEnd()) {
		uchar32_t currentChar = *iter;

		core::array<SGUITTGlyph>* glyphs = nullptr;
		core::array<CGUITTGlyphPage*>* glyphpages = nullptr;

		n = getGlyphIndexByChar(currentChar, &glyphs, &glyphpages);
		bool visible = (Invisible.findFirst(currentChar) == -1);
		if(n > 0 && visible) {
			bool lineBreak = false;
			if(currentChar == U'\r') { // Mac or Windows breaks
				lineBreak = true;
				const auto next = iter + 1;
				if(!next.atEnd() && *next == U'\n')	// Windows line breaks.
					currentChar = *(++iter);
			} else if(currentChar == U'\n') { // Unix breaks
				lineBreak = true;
			}

			if(lineBreak) {
				previousChar = 0;
				offset.Y += supposed_line_height; //font_metrics.ascender / 64;
				offset.X = position.UpperLeftCorner.X;

				if(hcenter)
					offset.X += (position.getWidth() - textDimension.Width) >> 1;
				++iter;
				continue;
			}

			// Calculate the glyph offset.
			s32 offx = glyphs->operator[](n - 1).offset.X;
			s32 offy = (font_metrics.ascender / 64) - glyphs->operator[](n - 1).offset.Y;

			// Apply kerning.
			core::vector2di k = getKerning(currentChar, previousChar);
			offset.X += k.X;
			offset.Y += k.Y;

			// Determine rendering information.
			SGUITTGlyph& glyph = glyphs->operator[](n - 1);
			CGUITTGlyphPage* const page = glyphpages->operator[](glyph.glyph_page);
			page->render_positions.push_back(core::vector2di(offset.X + offx, offset.Y + offy));
			page->render_source_rects.push_back(glyph.source_rect);
			Render_Map.insert(page);
		}
		offset.X += getWidthFromCharacter(currentChar);

		previousChar = currentChar;
		++iter;
	}

	// Draw now.
	update_glyph_pages();
	for(auto& page : Render_Map) {
		if(!use_transparency) color.color |= 0xff000000;
		Driver->draw2DImageBatch(page->texture, page->render_positions, page->render_source_rects, clip, color, true);
	}
}

core::dimension2d<u32> CGUITTFont::getCharDimension(const wchar_t ch) const {
	return core::dimension2d<u32>(getWidthFromCharacter(ch), getHeightFromCharacter(ch));
}

core::dimension2d<u32> CGUITTFont::getDimension(const wchar_t* text) const {
	return getDimensionustring(text);
}

core::dimension2d<u32> CGUITTFont::getDimension(const core::stringw& text) const {
	return getDimensionustring(text);
}

core::dimension2d<u32> CGUITTFont::getDimensionustring(const core::ustring& text) const {
	core::dimension2d<u32> text_dimension(0, supposed_line_height);
	core::dimension2d<u32> line(0, supposed_line_height);

	uchar32_t previousChar = 0;
	auto iter = text.begin();
	for(; !iter.atEnd(); ++iter) {
		uchar32_t p = *iter;
		bool lineBreak = false;
		if(p == U'\r') {	// Mac or Windows line breaks.
			lineBreak = true;
			auto next = iter + 1;
			if(!next.atEnd() && *next == U'\n') {
				++iter;
				p = *iter;
			}
		} else if(p == U'\n') {	// Unix line breaks.
			lineBreak = true;
		}

		// Kerning.
		core::vector2di k = getKerning(p, previousChar);
		line.Width += k.X;
		previousChar = p;

		// Check for linebreak.
		if(lineBreak) {
			previousChar = 0;
			text_dimension.Height += line.Height;
			if(text_dimension.Width < line.Width)
				text_dimension.Width = line.Width;
			line.Width = 0;
			line.Height = supposed_line_height;
			continue;
		}
		line.Width += getWidthFromCharacter(p);
	}
	if(text_dimension.Width < line.Width)
		text_dimension.Width = line.Width;

	return text_dimension;
}

inline u32 CGUITTFont::getWidthFromCharacter(wchar_t c) const {
	return getWidthFromCharacter((uchar32_t)c);
}

inline u32 CGUITTFont::getWidthFromCharacter(uchar32_t c) const {
	// Set the size of the face.
	// This is because we cache faces and the face may have been set to a different size.
	//FT_Set_Pixel_Sizes(tt_face, 0, size);

	core::array<SGUITTGlyph>* glyphs = nullptr;

	u32 n = getGlyphIndexByChar(c, &glyphs, nullptr);
	if(n > 0) {
		int w = glyphs->operator[](n - 1).advance.x / 64;
		return w;
	}
	if(c >= 0x2000)
		return (font_metrics.ascender / 64);
	else return (font_metrics.ascender / 64) / 2;
}

inline u32 CGUITTFont::getHeightFromCharacter(wchar_t c) const {
	return getHeightFromCharacter((uchar32_t)c);
}

inline u32 CGUITTFont::getHeightFromCharacter(uchar32_t c) const {
	// Set the size of the face.
	// This is because we cache faces and the face may have been set to a different size.
	//FT_Set_Pixel_Sizes(tt_face, 0, size);

	core::array<SGUITTGlyph>* glyphs = nullptr;

	u32 n = getGlyphIndexByChar(c, &glyphs, nullptr);
	if(n > 0) {
		// Grab the true height of the character, taking into account underhanging glyphs.
		s32 height = (font_metrics.ascender / 64) - glyphs->operator[](n - 1).offset.Y + glyphs->operator[](n - 1).source_rect.getHeight();
		return height;
	}
	if(c >= 0x2000)
		return (font_metrics.ascender / 64);
	else return (font_metrics.ascender / 64) / 2;
}

u32 CGUITTFont::getGlyphIndexByChar(wchar_t c, core::array<SGUITTGlyph>** glyphs, core::array<CGUITTGlyphPage*>** glyphpages, bool called_as_fallback) const {
	return getGlyphIndexByChar((uchar32_t)c, glyphs, glyphpages, called_as_fallback);
}

u32 CGUITTFont::getGlyphIndexByChar(uchar32_t c, core::array<SGUITTGlyph>** glyphs, core::array<CGUITTGlyphPage*>** glyphpages, bool called_as_fallback) const {
	// Get the glyph.
	u32 glyph_index = FT_Get_Char_Index(tt_face, c);

	if(glyph_index == 0 && fallback && glyphs) {
		glyph_index = fallback->getGlyphIndexByChar(c, glyphs, glyphpages, true);
		if(glyph_index)
			return glyph_index;
	}

	// Check for a valid glyph.  If it is invalid, attempt to use the replacement character.
	if(glyph_index == 0) {
		if(!called_as_fallback)
			glyph_index = FT_Get_Char_Index(tt_face, core::unicode::UTF_REPLACEMENT_CHARACTER);
		else
			return 0;
	}

	if(glyphs)
		*glyphs = &Glyphs;

	if(glyphpages)
		*glyphpages = &Glyph_Pages;

	// If our glyph is already loaded, don't bother doing any batch loading code.
	if(glyph_index != 0 && Glyphs[glyph_index - 1].isLoaded)
		return glyph_index;

	// Determine our batch loading positions.
	u32 half_size = (batch_load_size / 2);
	u32 start_pos = 0;
	if(c > half_size) start_pos = c - half_size;
	u32 end_pos = start_pos + batch_load_size;

	// Load all our characters.
	do {
		// Get the character we are going to load.
		u32 char_index = FT_Get_Char_Index(tt_face, start_pos);

		// If the glyph hasn't been loaded yet, do it now.
		if(char_index) {
			SGUITTGlyph& glyph = Glyphs[char_index - 1];
			if(!glyph.isLoaded) {
				glyph.preload(char_index, tt_face, Driver, size, load_flags);
				Glyph_Pages[glyph.glyph_page]->pushGlyphToBePaged(&glyph);
			}
		}
	} while(++start_pos < end_pos);

	// Return our original character.
	return glyph_index;
}

void CGUITTFont::clearGlyphPages() {
	for(u32 i = 0; i < Glyph_Pages.size(); ++i) {
		Glyph_Pages[i]->render_positions.clear();
		Glyph_Pages[i]->render_source_rects.clear();
	}
	if(fallback)
		fallback->clearGlyphPages();
}

s32 CGUITTFont::getCharacterFromPos(const wchar_t* text, s32 pixel_x) const {
	return getCharacterFromPos(core::ustring(text), pixel_x);
}

s32 CGUITTFont::getCharacterFromPos(const core::ustring& text, s32 pixel_x) const {
	s32 x = 0;
	//s32 idx = 0;

	u32 character = 0;
	uchar32_t previousChar = 0;
	for(auto c : text) {
		x += getWidthFromCharacter(c);

		// Kerning.
		core::vector2di k = getKerning(c, previousChar);
		x += k.X;

		if(x >= pixel_x)
			return character;

		previousChar = c;
		++character;
		if constexpr(sizeof(wchar_t) == 2) {
			if(c >= 0x10000)
				++character;
		}
	}

	return -1;
}

void CGUITTFont::setKerningWidth(s32 kerning) {
	GlobalKerningWidth = kerning;
}

void CGUITTFont::setKerningHeight(s32 kerning) {
	GlobalKerningHeight = kerning;
}

s32 CGUITTFont::getKerningWidth(const wchar_t* thisLetter, const wchar_t* previousLetter) const {
	if(tt_face == 0)
		return GlobalKerningWidth;
	if(thisLetter == 0 || previousLetter == 0)
		return 0;

	return getKerningWidth((uchar32_t)* thisLetter, (uchar32_t)* previousLetter);
}

s32 CGUITTFont::getKerningWidth(const uchar32_t thisLetter, const uchar32_t previousLetter) const {
	// Return only the kerning width.
	return getKerning(thisLetter, previousLetter).X;
}

s32 CGUITTFont::getKerningHeight() const {
	// FreeType 2 currently doesn't return any height kerning information.
	return GlobalKerningHeight;
}

core::vector2di CGUITTFont::getKerning(const wchar_t thisLetter, const wchar_t previousLetter) const {
	return getKerning((uchar32_t)thisLetter, (uchar32_t)previousLetter);
}

core::vector2di CGUITTFont::getKerning(const uchar32_t thisLetter, const uchar32_t previousLetter) const {
	if(tt_face == 0 || thisLetter == 0 || previousLetter == 0)
		return core::vector2di();

	// Set the size of the face.
	// This is because we cache faces and the face may have been set to a different size.
	FT_Set_Pixel_Sizes(tt_face, 0, size);

	core::vector2di ret(GlobalKerningWidth, GlobalKerningHeight);

	// If we don't have kerning, no point in continuing.
	if(!FT_HAS_KERNING(tt_face))
		return ret;

	// Get the kerning information.
	FT_Vector v;
	FT_Get_Kerning(tt_face, getGlyphIndexByChar(previousLetter, nullptr, nullptr), getGlyphIndexByChar(thisLetter, nullptr, nullptr), FT_KERNING_DEFAULT, &v);

	// If we have a scalable font, the return value will be in font points.
	if(FT_IS_SCALABLE(tt_face)) {
		// Font points, so divide by 64.
		ret.X += (v.x / 64);
		ret.Y += (v.y / 64);
	} else {
		// Pixel units.
		ret.X += v.x;
		ret.Y += v.y;
	}
	return ret;
}

void CGUITTFont::setInvisibleCharacters(const wchar_t *s) {
	Invisible_w = s;
	Invisible = Invisible_w;
}

video::IImage* CGUITTFont::createTextureFromChar(const uchar32_t& ch) {
	core::array<SGUITTGlyph>* glyphs = nullptr;
	core::array<CGUITTGlyphPage*>* glyphpages = nullptr;

	u32 n = getGlyphIndexByChar(ch, &glyphs, &glyphpages);
	const SGUITTGlyph& glyph = glyphs->operator[](n - 1);
	CGUITTGlyphPage* page = glyphpages->operator[](glyph.glyph_page);

	if(page->dirty)
		page->updateTexture();

	video::ITexture* tex = page->texture;

	// Acquire a read-only lock of the corresponding page texture.
	void* ptr = tex->lock(video::ETLM_READ_ONLY);

	video::ECOLOR_FORMAT format = tex->getColorFormat();
	core::dimension2du tex_size = tex->getOriginalSize();
	video::IImage* pageholder = Driver->createImageFromData(format, tex_size, ptr, true, false);

	// Copy the image data out of the page texture.
	core::dimension2du glyph_size(glyph.source_rect.getSize());
	video::IImage* image = Driver->createImage(format, glyph_size);
	pageholder->copyTo(image, core::vector2di(0, 0), glyph.source_rect);

	tex->unlock();
	return image;
}

video::ITexture* CGUITTFont::getPageTextureByIndex(const u32& page_index) const {
	if(page_index < Glyph_Pages.size())
		return Glyph_Pages[page_index]->texture;
	return nullptr;
}

CGUITTGlyphPage::~CGUITTGlyphPage() {
	if(texture) {
		if(driver)
			driver->removeTexture(texture);
		else texture->drop();
	}
}

bool CGUITTGlyphPage::createPageTexture(const u8 & pixel_mode, const core::dimension2du & target_texture_size) {
	if(texture)
		return false;

	bool flgmip = driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);

	// Set the texture color format.
	switch(pixel_mode) {
		case FT_PIXEL_MODE_MONO:
			texture = driver->addTexture(target_texture_size, name, video::ECF_A1R5G5B5);
			break;
		case FT_PIXEL_MODE_GRAY:
		default:
			texture = driver->addTexture(target_texture_size, name, video::ECF_A8R8G8B8);
			break;
	}

	// Restore our texture creation flags.
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, flgmip);
	return texture ? true : false;
}

void CGUITTGlyphPage::updateTexture() {
	if(!dirty) return;

	void* ptr = texture->lock();
	video::ECOLOR_FORMAT format = texture->getColorFormat();
	core::dimension2du size = texture->getOriginalSize();
	video::IImage* pageholder = driver->createImageFromData(format, size, ptr, true, false);

	for(u32 i = 0; i < glyph_to_be_paged.size(); ++i) {
		const SGUITTGlyph* glyph = glyph_to_be_paged[i];
		if(glyph && glyph->isLoaded) {
			if(glyph->surface) {
				glyph->surface->copyTo(pageholder, glyph->source_rect.UpperLeftCorner);
				glyph->surface->drop();
				glyph->surface = nullptr;
			} else {
				; // TODO: add error message?
				//currently, if we failed to create the image, just ignore this operation.
			}
		}
	}

	pageholder->drop();
	texture->unlock();
	glyph_to_be_paged.clear();
	dirty = false;
}

} // end namespace gui
} // end namespace irr
