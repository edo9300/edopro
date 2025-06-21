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

#ifndef __C_GUI_TTFONT_H_INCLUDED__
#define __C_GUI_TTFONT_H_INCLUDED__

#include <ft2build.h>
#include FT_FREETYPE_H
#include <list>
#include <string>
#include <IGUIFont.h>
#include "irrUString.h"
#include <rect.h>
#include <SMesh.h>
#include <irrMap.h>
#include <path.h>
#include "../text_types.h"

namespace irr {
class IrrlichtDevice;
namespace video {
class IVideoDriver;
class IImage;
class ITexture;
}
namespace gui {
struct SGUITTFace;
class CGUITTFont;
class IGUIEnvironment;

//! Class to assist in deleting glyphs.
class CGUITTAssistDelete {
public:
	template <class T, typename TAlloc>
	static void Delete(core::array<T, TAlloc>& a) {
		TAlloc allocator;
		allocator.deallocate(a.pointer());
	}
};

//! Structure representing a single TrueType glyph.
struct SGUITTGlyph {
	//! Constructor.
	SGUITTGlyph() : isLoaded(false), glyph_page(0), advance({}), surface(0), parent(0) {}

	//! Destructor.
	~SGUITTGlyph() {
		unload();
	}

	//! Preload the glyph.
	//!	The preload process occurs when the program tries to cache the glyph from FT_Library.
	//! However, it simply defines the SGUITTGlyph's properties and will only create the page
	//! textures if necessary.  The actual creation of the textures should only occur right
	//! before the batch draw call.
	void preload(u32 char_index, FT_Face face, video::IVideoDriver* driver, u32 font_size, const FT_Int32 loadFlags);

	//! Unloads the glyph.
	void unload();

	//! Creates the IImage object from the FT_Bitmap.
	video::IImage* createGlyphImage(const FT_Bitmap& bits, video::IVideoDriver* driver) const;

	//! If true, the glyph has been loaded.
	bool isLoaded;

	//! The page the glyph is on.
	u32 glyph_page;

	//! The source rectangle for the glyph.
	core::recti source_rect;

	//! The offset of glyph when drawn.
	core::vector2di offset;

	//! Glyph advance information.
	FT_Vector advance;

	//! This is just the temporary image holder.  After this glyph is paged,
	//! it will be dropped.
	mutable video::IImage* surface;

	//! The pointer pointing to the parent (CGUITTFont)
	CGUITTFont* parent;
};

//! Holds a sheet of glyphs.
class CGUITTGlyphPage {
public:
	CGUITTGlyphPage(video::IVideoDriver* Driver, const std::string& texture_name) :
		texture(0), available_slots(0), used_slots(0), dirty(false), driver(Driver),
		name({ texture_name.data(), (irr::u32)texture_name.size() }), preloaded_pixel_mode(0), preloaded_texture_size({}) {}
	~CGUITTGlyphPage();

	//! Create the actual page texture,
	bool createPageTexture(const u8& pixel_mode, const core::dimension2du& texture_size);

	//! Create the actual page texture,
	bool createPreloadedPageTexture() { return createPageTexture(preloaded_pixel_mode, preloaded_texture_size); }

	//! Create the actual page texture,
	void preloadPageTexture(const u8& pixel_mode, const core::dimension2du& texture_size);

	//! Add the glyph to a list of glyphs to be paged.
	//! This collection will be cleared after updateTexture is called.
	void pushGlyphToBePaged(const SGUITTGlyph* glyph) {
		glyph_to_be_paged.push_back(glyph);
	}

	//! Updates the texture atlas with new glyphs.
	void updateTexture();

	video::ITexture* texture;
	u32 available_slots;
	u32 used_slots;
	bool dirty;

	core::array<core::vector2di> render_positions;
	core::array<core::recti> render_source_rects;

	core::dimension2du texture_size;
private:
	core::array<const SGUITTGlyph*> glyph_to_be_paged;
	video::IVideoDriver* driver;
	io::path name;
	u8 preloaded_pixel_mode;
	core::dimension2du preloaded_texture_size;
};

//! Class representing a TrueType font.
class CGUITTFont : public IGUIFont {
public:
	//! Creates a new TrueType font and returns a pointer to it.  The pointer must be drop()'ed when finished.
	//! \param env The IGUIEnvironment the font loads out of.
	//! \param filename The filename of the font.
	//! \param size The size of the font glyphs in pixels.  Since this is the size of the individual glyphs, the true height of the font may change depending on the characters used.
	//! \param antialias set the use_monochrome (opposite to antialias) flag
	//! \param transparency set the use_transparency flag
	//! \return Returns a pointer to a CGUITTFont.  Will return 0 if the font failed to load.
	struct FontInfo {
		const io::path font;
		const u32 size;
		template<typename T>
		FontInfo(const T& other) : font({ other.font.data(), (u32)other.font.size() }), size(other.size) {}
		FontInfo(epro::path_stringview _filename, u32 _size) : font({ _filename.data(), (u32)_filename.size() }), size(_size) {}
	};
	using FallbackFonts = std::list<FontInfo>;
	template<typename T>
	static CGUITTFont* createTTFont(IGUIEnvironment* env, const FontInfo& font_info, const T& fallback, const bool antialias = true, const bool transparency = true) {
		FallbackFonts fonts;
		for(const auto& font : fallback)
			fonts.emplace_back(font);
		return createTTFont(nullptr, env, font_info, fonts.begin(), fonts.end(), antialias, transparency);
	}
	static CGUITTFont* createTTFont(IrrlichtDevice* device, IGUIEnvironment* env, const FontInfo& font_info, FallbackFonts::const_iterator fallback_begin, FallbackFonts::const_iterator fallback_end, const bool antialias = true, const bool transparency = true);

	//! Destructor
	~CGUITTFont() override;

	//! Sets the amount of glyphs to batch load.
	void setBatchLoadSize(u32 batch_size) {
		batch_load_size = batch_size;
	}

	//! Sets the maximum texture size for a page of glyphs.
	void setMaxPageTextureSize(const core::dimension2du& texture_size) {
		max_page_texture_size = texture_size;
	}

	//! Get the font size.
	u32 getFontSize() const {
		return size;
	}

	//! Check the font's transparency.
	bool isTransparent() const {
		return use_transparency;
	}

	//! Check if the font auto-hinting is enabled.
	//! Auto-hinting is FreeType's built-in font hinting engine.
	bool useAutoHinting() const {
		return use_auto_hinting;
	}

	//! Check if the font hinting is enabled.
	bool useHinting() const {
		return use_hinting;
	}

	//! Check if the font is being loaded as a monochrome font.
	//! The font can either be a 256 color grayscale font, or a 2 color monochrome font.
	bool useMonochrome() const {
		return use_monochrome;
	}

	//! Tells the font to allow transparency when rendering.
	//! Default: true.
	//! \param flag If true, the font draws using transparency.
	void setTransparency(const bool flag);

	//! Tells the font to use monochrome rendering.
	//! Default: false.
	//! \param flag If true, the font draws using a monochrome image.  If false, the font uses a grayscale image.
	void setMonochrome(const bool flag);

	//! Enables or disables font hinting.
	//! Default: Hinting and auto-hinting true.
	//! \param enable If false, font hinting is turned off. If true, font hinting is turned on.
	//! \param enable_auto_hinting If true, FreeType uses its own auto-hinting algorithm.  If false, it tries to use the algorithm specified by the font.
	void setFontHinting(const bool enable, const bool enable_auto_hinting = true);

	//! Draws some text and clips it to the specified rectangle if wanted.
	void draw(const core::stringw& text, const core::rect<s32>& position,
					  video::SColor color, bool hcenter = false, bool vcenter = false,
					  const core::rect<s32>* clip = 0) override;

	void drawustring(const core::ustring& text, const core::rect<s32>& position,
					  video::SColor color, bool hcenter = false, bool vcenter = false,
					  const core::rect<s32>* clip = 0);

	//! Returns the dimension of a character produced by this font.
	core::dimension2d<u32> getCharDimension(const wchar_t ch) const;

	//! Returns the dimension of a text string.
	core::dimension2d<u32> getDimension(const wchar_t* text) const override;
	core::dimension2d<u32> getDimension(const core::stringw& text) const override;
	core::dimension2d<u32> getDimensionustring(const core::ustring& text) const;

	//! Calculates the index of the character in the text which is on a specific position.
	s32 getCharacterFromPos(const wchar_t* text, s32 pixel_x) const override;
	s32 getCharacterFromPos(const core::ustring& text, s32 pixel_x) const;

	//! Sets global kerning width for the font.
	void setKerningWidth(s32 kerning) override;

	//! Sets global kerning height for the font.
	void setKerningHeight(s32 kerning) override;

	//! Gets kerning values (distance between letters) for the font. If no parameters are provided,
	s32 getKerningWidth(const wchar_t* thisLetter = 0, const wchar_t* previousLetter = 0) const override;
	s32 getKerningWidth(const uchar32_t thisLetter = 0, const uchar32_t previousLetter = 0) const;

	//! Returns the distance between letters
	s32 getKerningHeight() const override;

	//! Define which characters should not be drawn by the font.
	void setInvisibleCharacters(const wchar_t *s) override;

	//! Get the last glyph page if there's still available slots.
	//! If not, it will return zero.
	CGUITTGlyphPage* getLastGlyphPage() const;

	//! Create a new glyph page texture.
	//! \param pixel_mode the pixel mode defined by FT_Pixel_Mode
	//should be better typed. fix later.
	CGUITTGlyphPage* createGlyphPage(const u8& pixel_mode);

	//! Get the last glyph page's index.
	u32 getLastGlyphPageIndex() const {
		return Glyph_Pages.size() - 1;
	}

	//! Create corresponding character's software image copy from the font,
	//! so you can use this data just like any ordinary video::IImage.
	//! \param ch The character you need
	video::IImage* createTextureFromChar(const uchar32_t& ch);

	//! This function is for debugging mostly. If the page doesn't exist it returns zero.
	//! \param page_index Simply return the texture handle of a given page index.
	video::ITexture* getPageTextureByIndex(const u32& page_index) const;

	u32 getGlyphIndexByChar(wchar_t c, core::array<SGUITTGlyph>** glyphs, core::array<CGUITTGlyphPage*>** glyphpages, bool called_as_fallback = false) const;
	u32 getGlyphIndexByChar(uchar32_t c, core::array<SGUITTGlyph>** glyphs, core::array<CGUITTGlyphPage*>** glyphpages, bool called_as_fallback = false) const;

	void clearGlyphPages();

protected:
	bool use_monochrome;
	bool use_transparency;
	bool use_hinting;
	bool use_auto_hinting;
	u32 size;
	u32 batch_load_size;
	core::dimension2du max_page_texture_size;

private:
	// Manages the FreeType library.
	static FT_Library c_library;
	static core::map<io::path, SGUITTFace*> c_faces;

	explicit CGUITTFont(IGUIEnvironment *env);
	bool load(const io::path& filename, const u32 size, const bool antialias, const bool transparency);
	void reset_images();
	void update_glyph_pages() const;
	void update_load_flags() {
		// Set up our loading flags.
		load_flags = FT_LOAD_DEFAULT | FT_LOAD_RENDER;
		if(!useHinting()) load_flags |= FT_LOAD_NO_HINTING;
		if(!useAutoHinting()) load_flags |= FT_LOAD_NO_AUTOHINT;
		if(useMonochrome()) load_flags |= FT_LOAD_MONOCHROME | FT_LOAD_TARGET_MONO | FT_RENDER_MODE_MONO;
		else load_flags |= FT_LOAD_TARGET_NORMAL;
	}
	u32 getWidthFromCharacter(wchar_t c) const;
	u32 getWidthFromCharacter(uchar32_t c) const;
	u32 getHeightFromCharacter(wchar_t c) const;
	u32 getHeightFromCharacter(uchar32_t c) const;
	core::vector2di getKerning(const wchar_t thisLetter, const wchar_t previousLetter) const;
	core::vector2di getKerning(const uchar32_t thisLetter, const uchar32_t previousLetter) const;

	irr::IrrlichtDevice* Device;
	gui::IGUIEnvironment* Environment;
	video::IVideoDriver* Driver;
	io::path filename;
	FT_Face tt_face;
	FT_Size_Metrics font_metrics;
	FT_Int32 load_flags;

	mutable core::array<CGUITTGlyphPage*> Glyph_Pages;
	mutable core::array<SGUITTGlyph> Glyphs;

	s32 GlobalKerningWidth;
	s32 GlobalKerningHeight;
	s32 supposed_line_height;
	core::ustring Invisible;
	core::stringw Invisible_w;

	CGUITTFont* fallback;
};

} // end namespace gui
} // end namespace irr

#endif // __C_GUI_TTFONT_H_INCLUDED__
