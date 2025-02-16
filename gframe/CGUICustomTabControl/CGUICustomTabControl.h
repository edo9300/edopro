// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef C_GUI_CUSTOM_TAB_CONTROL_H
#define C_GUI_CUSTOM_TAB_CONTROL_H

#include <IrrCompileConfig.h>
#ifdef _IRR_COMPILE_WITH_GUI_

#include <IGUITabControl.h>
#if IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9
#include "../IrrlichtCommonIncludes1.9/CGUITabControl.h"
#else
#include "../IrrlichtCommonIncludes/CGUITabControl.h"
#endif
#include <irrArray.h>

namespace irr {
namespace gui {
//! A standard tab control
class CGUICustomTabControl final : public IGUITabControl {
public:

	//! destructor
	CGUICustomTabControl(IGUIEnvironment* environment,
						 IGUIElement* parent, const core::rect<s32>& rectangle,
						 bool fillbackground = true, bool border = true, s32 id = -1);

	static IGUITabControl* addCustomTabControl(IGUIEnvironment* environment, const core::rect<s32>& rectangle,
											   IGUIElement* parent, bool fillbackground = false, bool border = true, s32 id = -1);

	//! destructor
	~CGUICustomTabControl() override;

	//! Adds a tab
	IGUITab* addTab(const wchar_t* caption, s32 id = -1) override;

#if IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9
	//! Adds an existing tab
	s32 addTab(IGUITab* tab) override { return -1; };

	//! Insert an existing tab
	/** Note that it will also add the tab as a child of this TabControl.
	\return Index of added tab (should be same as the one passed) or -1 for failure*/
	s32 insertTab(s32 idx, IGUITab* tab, bool serializationMode) override { return -1; };
#else
	//! Adds a tab that has already been created
	virtual void addTab(CGUITab* tab);
#endif

	//! Insert the tab at the given index
	IGUITab* insertTab(s32 idx, const wchar_t* caption, s32 id = -1) override;

	//! Removes a tab from the tabcontrol
	void removeTab(s32 idx) override;

	//! Clears the tabcontrol removing all tabs
	void clear() override;

	//! Returns amount of tabs in the tabcontrol
	s32 getTabCount() const override;

	//! Returns a tab based on zero based index
	IGUITab* getTab(s32 idx) const override;

	//! Brings a tab to front.
	bool setActiveTab(s32 idx) override;

	//! Brings a tab to front.
	bool setActiveTab(IGUITab *tab) override;

	//! Returns which tab is currently active
	s32 getActiveTab() const override;

	//! get the the id of the tab at the given absolute coordinates
	s32 getTabAt(s32 xpos, s32 ypos) const override;

	//! called if an event happened.
	bool OnEvent(const SEvent& event) override;

	//! draws the element and its children
	void draw() override;

	//! Removes a child.
	void removeChild(IGUIElement* child) override;

	//! Writes attributes of the element.
	void serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options) const override;
	//! Set the height of the tabs
	void setTabHeight(s32 height) override;

	//! Reads attributes of the element
	void deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options) override;

	//! Get the height of the tabs
	s32 getTabHeight() const override;

	//! set the maximal width of a tab. Per default width is 0 which means "no width restriction".
	void setTabMaxWidth(s32 width) override;

	//! get the maximal width of a tab
	s32 getTabMaxWidth() const override;

	//! Set the alignment of the tabs
	//! note: EGUIA_CENTER is not an option
	void setTabVerticalAlignment(gui::EGUI_ALIGNMENT alignment) override;

	//! Get the alignment of the tabs
	gui::EGUI_ALIGNMENT getTabVerticalAlignment() const override;

	//! Set the extra width added to tabs on each side of the text
	void setTabExtraWidth(s32 extraWidth) override;

	//! Get the extra width added to tabs on each side of the text
	s32 getTabExtraWidth() const override;

	//! Update the position of the element, decides scroll button status
	void updateAbsolutePosition() override;

#if IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9
	//! For given given tab find it's zero-based index (or -1 for not found)
	s32 getTabIndex(const IGUIElement *tab) const override;
#endif

private:

	void scrollLeft();
	void scrollRight();
	bool needScrollControl(s32 startIndex = 0, bool withScrollControl = false);
	s32 calcTabWidth(s32 pos, IGUIFont* font, const wchar_t* text, bool withScrollControl) const;
	core::rect<s32> calcTabPos();

	void recalculateScrollButtonPlacement();
	void recalculateScrollBar();
	void refreshSprites();

	core::array<CGUITab*> Tabs;	// CGUITab* because we need setNumber (which is certainly not nice)
	s32 ActiveTab;
	bool Border;
	bool FillBackground;
	bool ScrollControl;
	s32 TabHeight;
	gui::EGUI_ALIGNMENT VerticalAlignment;
	IGUIButton* UpButton;
	IGUIButton* DownButton;
	s32 TabMaxWidth;
	s32 CurrentScrollTabIndex;
	s32 TabExtraWidth;
};


} // end namespace gui
} // end namespace irr

#endif // _IRR_COMPILE_WITH_GUI_

#endif // C_GUI_CUSTOM_TAB_CONTROL_H

