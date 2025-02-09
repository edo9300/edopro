#ifndef C_GUI_CUSTOM_TEXT_H
#define C_GUI_CUSTOM_TEXT_H

#include <IrrCompileConfig.h>
#ifdef _IRR_COMPILE_WITH_GUI_

#include <IGUIStaticText.h>
#include <irrArray.h>

namespace irr {
namespace gui {

class IGUIScrollBar;

class CGUICustomText final : public IGUIStaticText {
public:
	enum CTEXT_SCROLLING_TYPE {

		NO_SCROLLING = 0,

		LEFT_TO_RIGHT,

		RIGHT_TO_LEFT,

		TOP_TO_BOTTOM,

		BOTTOM_TO_TOP,

		LEFT_TO_RIGHT_BOUNCING,

		RIGHT_TO_LEFT_BOUNCING,

		TOP_TO_BOTTOM_BOUNCING,

		BOTTOM_TO_TOP_BOUNCING

	};

	//! constructor
	CGUICustomText(const wchar_t* text, bool border, IGUIEnvironment* environment, IGUIElement* parent, s32 id,
				   const core::rect<s32>& rectangle, bool background = false);

	static CGUICustomText* addCustomText(const wchar_t* text, bool border, IGUIEnvironment* environment, IGUIElement* parent, s32 id,
										 const core::rect<s32>& rectangle, bool background = false);

	//! destructor
	~CGUICustomText() override;

	//! called if an event happened.
	bool OnEvent(const SEvent& event) override;

	//! draws the element and its children
	void draw() override;

	//! Sets another skin independent font.
	void setOverrideFont(IGUIFont* font = 0) override;

	//! Gets the override font (if any)
	IGUIFont* getOverrideFont() const override;

	//! Get the font which is used right now for drawing
	IGUIFont* getActiveFont() const override;

	//! Sets another color for the text.
	void setOverrideColor(video::SColor color) override;

	//! Sets another color for the background.
	void setBackgroundColor(video::SColor color) override;

	//! Sets whether to draw the background
	void setDrawBackground(bool draw) override;

	//! Gets the background color
	video::SColor getBackgroundColor() const override;

	//! Checks if background drawing is enabled
	bool isDrawBackgroundEnabled() const override;

	//! Sets whether to draw the border
	void setDrawBorder(bool draw) override;

	//! Checks if border drawing is enabled
	bool isDrawBorderEnabled() const override;

	//! Sets alignment mode for text
	void setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical) override;

	//! Gets the override color
	video::SColor getOverrideColor() const override;

	//! Sets if the static text should use the overide color or the
	//! color in the gui skin.
	void enableOverrideColor(bool enable) override;

	//! Checks if an override color is enabled
	bool isOverrideColorEnabled() const override;

	//! Set whether the text in this label should be clipped if it goes outside bounds
	void setTextRestrainedInside(bool restrainedInside) override;

	//! Checks if the text in this label should be clipped if it goes outside bounds
	bool isTextRestrainedInside() const override;

	//! Enables or disables word wrap for using the static text as
	//! multiline text control.
	void setWordWrap(bool enable) override;

	//! Checks if word wrap is enabled
	bool isWordWrapEnabled() const override;

	//! Sets the new caption of this element.
	void setText(const wchar_t* text) override;

	//! Returns the height of the text in pixels when it is drawn.
	s32 getTextHeight() const override;

	//! Returns the width of the current text, in the current font
	s32 getTextWidth() const override;

	//! Updates the absolute position, splits text if word wrap is enabled
	void updateAbsolutePosition() override;

	//! Set whether the string should be interpreted as right-to-left (RTL) text
	/** \note This component does not implement the Unicode bidi standard, the
	text of the component should be already RTL if you call this. The
	main difference when RTL is enabled is that the linebreaks for multiline
	elements are performed starting from the end.
	*/
	void setRightToLeft(bool rtl) override;

	//! Checks if the text should be interpreted as right-to-left text
	bool isRightToLeft() const override;

	//! Writes attributes of the element.
	void serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options) const override;

	//! Reads attributes of the element
	void deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options) override;

	void enableScrollBar(int scroll_width = 0, float scroll_ratio = 0);

	irr::gui::IGUIScrollBar* getScrollBar();

	bool hasScrollBar();

	void setTextAutoScrolling(CTEXT_SCROLLING_TYPE type, int frames, float steps_ratio = 0.0f, int steps = 0, int waitstart = 0, int waitend = 0);

	bool hasVerticalAutoscrolling() const;

	bool hasHorizontalAutoscrolling() const;

#if IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9
	//! Gets the currently used text color
	video::SColor getActiveColor() const override;
#else
	video::SColor getActiveColor() const;
#endif

private:

	//! Breaks the single text line.
	void breakText();
	void breakText(bool scrollbar_spacing);

	EGUI_ALIGNMENT HAlign, VAlign;
	bool Border;
	bool OverrideColorEnabled;
	bool OverrideBGColorEnabled;
	bool WordWrap;
	bool Background;
	bool RestrainTextInside;
	bool RightToLeft;
	bool was_pressed;
	core::vector2di prev_position;

	video::SColor OverrideColor, BGColor;
	gui::IGUIFont* OverrideFont;
	gui::IGUIFont* LastBreakFont; // stored because: if skin changes, line break must be recalculated.

	core::array< core::stringw > BrokenText;

	// scrollbar related variables
	irr::gui::IGUIScrollBar* scrText;
	int ScrollWidth;
	float ScrollRatio;

	// auto scrolling related functions and variables
	CTEXT_SCROLLING_TYPE scrolling;
	float maxFrame;
	float curFrame;
	float frameTimer;
	float forcedSteps;
	float forcedStepsRatio;
	float animationStep;
	float animationWaitStart;
	float animationWaitEnd;
	bool increasingFrame;
	bool waitingEndFrame;

	u32 prev_time;

	void updateAutoScrollingStuff();
};

} // end namespace gui
} // end namespace irr

#endif // _IRR_COMPILE_WITH_GUI_

#endif // C_GUI_CUSTOM_TEXT_H

