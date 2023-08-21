#ifndef CIMAGEGUISKIN_H_
#define CIMAGEGUISKIN_H_

#include <IGUISkin.h>
#include <irrString.h>
#include <irrMap.h>
#include "../custom_skin_enum.h"

namespace irr {
namespace video {
class IVideoDriver;
class ITexture;
}
namespace gui {

class IGUISpriteBank;

struct SImageGUIElementStyle {
	struct SBorder {
		s32 Top, Left, Bottom, Right;
		SBorder() : Top(0), Left(0), Bottom(0), Right(0) {}
	};
	SBorder SrcBorder;
	SBorder DstBorder;
	video::ITexture* Texture;
	video::SColor Color;

	SImageGUIElementStyle() : Texture(0), Color(255, 255, 255, 255) {}
};

struct SImageGUISkinConfig {
	SImageGUIElementStyle SunkenPane, Window, Button, ButtonPressed, ButtonDisabled, ProgressBar, ProgressBarFilled, TabButton, TabButtonPressed, TabBody, MenuBar, MenuPane, MenuPressed, CheckBox, CheckBoxDisabled, ComboBox, ComboBoxDisabled;
	video::SColor CheckBoxColor;
};

class CImageGUISkin final : public IGUISkin {
public:
	CImageGUISkin(video::IVideoDriver* videoDriver, IGUISkin* fallbackSkin);
	~CImageGUISkin() override;

	void loadConfig(const SImageGUISkinConfig& config);

	//! returns default color
	video::SColor getColor(EGUI_DEFAULT_COLOR color) const override;

	//! sets a default color
	void setColor(EGUI_DEFAULT_COLOR which, video::SColor newColor) override;

	//! returns default color
	s32 getSize(EGUI_DEFAULT_SIZE size) const override;

	//! Returns a default text. 
	const wchar_t* getDefaultText(EGUI_DEFAULT_TEXT text) const override;

	//! Sets a default text.
	void setDefaultText(EGUI_DEFAULT_TEXT which, const wchar_t* newText) override;

	//! sets a default size
	void setSize(EGUI_DEFAULT_SIZE which, s32 size) override;

	//! returns the default font
	IGUIFont* getFont(EGUI_DEFAULT_FONT defaultFont) const override;

	//! sets a default font
	void setFont(IGUIFont* font, EGUI_DEFAULT_FONT defaultFont) override;

	//! returns the sprite bank
	IGUISpriteBank* getSpriteBank() const override;

	//! sets the sprite bank
	void setSpriteBank(IGUISpriteBank* bank) override;

	u32 getIcon(EGUI_DEFAULT_ICON icon) const override;

	void setIcon(EGUI_DEFAULT_ICON icon, u32 index) override;

	virtual void draw3DButtonPaneStandard(IGUIElement* element,
										  const core::rect<s32>& rect,
										  const core::rect<s32>* clip = 0);

	virtual void draw3DButtonPaneDisabled(IGUIElement* element,
										  const core::rect<s32>& rect,
										  const core::rect<s32>* clip = 0);

	virtual void draw3DButtonPanePressed(IGUIElement* element,
										 const core::rect<s32>& rect,
										 const core::rect<s32>* clip = 0);

	virtual void draw3DSunkenPane(IGUIElement* element,
								  video::SColor bgcolor, bool flat, bool fillBackGround,
								  const core::rect<s32>& rect,
								  const core::rect<s32>* clip = 0);
	/* Updates for irrlicht 1.7 by Mamnarock
	virtual core::rect<s32> draw3DWindowBackground(IGUIElement* element,
			bool drawTitleBar, video::SColor titleBarColor,
			const core::rect<s32>& rect,
			const core::rect<s32>* clip=0);
  */
	virtual core::rect<s32> draw3DWindowBackground(IGUIElement* element,
												   bool drawTitleBar, video::SColor titleBarColor,
												   const core::rect<s32>& rect,
												   const core::rect<s32>* clip = 0,
												   core::rect<s32>* checkClientArea = 0);

	virtual void draw3DMenuPane(IGUIElement* element,
								const core::rect<s32>& rect,
								const core::rect<s32>* clip = 0);

	virtual void draw3DToolBar(IGUIElement* element,
							   const core::rect<s32>& rect,
							   const core::rect<s32>* clip = 0);

	virtual void draw3DTabButton(IGUIElement* element, bool active,
								 const core::rect<s32>& rect, const core::rect<s32>* clip = 0, EGUI_ALIGNMENT alignment = EGUIA_UPPERLEFT);

	virtual void draw3DTabBody(IGUIElement* element, bool border, bool background,
							   const core::rect<s32>& rect, const core::rect<s32>* clip = 0, s32 tabHeight = -1, gui::EGUI_ALIGNMENT alignment = EGUIA_UPPERLEFT);

	virtual void drawIcon(IGUIElement* element, EGUI_DEFAULT_ICON icon,
						  const core::vector2di position, u32 starttime = 0, u32 currenttime = 0,
						  bool loop = false, const core::rect<s32>* clip = 0);
	// Madoc - I had to add some things

	// Exposes config so we can get the progress bar colors
	SImageGUISkinConfig getConfig() { return Config; }

	// End Madoc adds
	virtual void drawHorizontalProgressBar(IGUIElement* element, const core::rect<s32>& rectangle, const core::rect<s32>* clip,
										   f32 filledRatio, video::SColor fillColor, video::SColor emptyColor);

	virtual void draw2DRectangle(IGUIElement* element, const video::SColor &color,
								 const core::rect<s32>& pos, const core::rect<s32>* clip = 0);
	void setProperty(core::stringw key, core::stringw value);
	core::stringw getProperty(core::stringw key);
	void setCustomColor(ygo::skin::CustomSkinElements key, video::SColor value);
	video::SColor getCustomColor(ygo::skin::CustomSkinElements key, video::SColor fallback);

private:
	void drawElementStyle(const SImageGUIElementStyle& elem, const core::rect<s32>& rect, const core::rect<s32>* clip, video::SColor* color = 0);

	video::IVideoDriver* VideoDriver;
	IGUISkin* FallbackSkin;
	SImageGUISkinConfig Config;

	core::map<core::stringw, core::stringw> properties;
	core::map<ygo::skin::CustomSkinElements, video::SColor> custom_colors;
};

}
}

#endif


