// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef C_GUI_CUSTOM_CONTEXT_MENU_H
#define C_GUI_CUSTOM_CONTEXT_MENU_H

#include <IrrCompileConfig.h>
#ifdef _IRR_COMPILE_WITH_GUI_

#include <IGUIContextMenu.h>
#include <irrString.h>
#include <irrArray.h>

namespace irr {
namespace gui {

class IGUIFont;
class IGUIScrollBar;

//! GUI Context menu interface.
class CGUICustomContextMenu : public IGUIContextMenu {
public:

	//! constructor
	CGUICustomContextMenu(IGUIEnvironment* environment,
						  IGUIElement* parent, s32 id, core::rect<s32> rectangle,
						  bool getFocus = true, bool allowFocus = true, core::rect<s32>* maxRect = nullptr);

	//! destructor
	~CGUICustomContextMenu() override;

	static IGUIContextMenu* addCustomContextMenu(IGUIEnvironment* environment, IGUIElement* parent, s32 id, core::rect<s32> rectangle, bool getFocus = true, bool allowFocus = true, core::rect<s32>* maxRect = nullptr);

	//! set behavior when menus are closed
	void setCloseHandling(ECONTEXT_MENU_CLOSE onClose) override;

	//! get current behavior when the menue will be closed
	ECONTEXT_MENU_CLOSE getCloseHandling() const override;

	//! Returns amount of menu items
	u32 getItemCount() const override;

	//! Adds a menu item.
	u32 addItem(const wchar_t* text, s32 commandid,
						bool enabled, bool hasSubMenu, bool checked, bool autoChecking) override;

	// Adds an item of "any" type via reference, this item won't have the other properties as submenus etc
	u32 addItem(IGUIElement* element, s32 commandid);

	u32 insertItem(u32 idx, IGUIElement* element, s32 commandid);

	//! Insert a menu item at specified position.
	u32 insertItem(u32 idx, const wchar_t* text, s32 commandId, bool enabled,
						   bool hasSubMenu, bool checked, bool autoChecking) override;

	//! Find a item which has the given CommandId starting from given index
	s32 findItemWithCommandId(s32 commandId, u32 idxStartSearch) const override;

	//! Adds a separator item to the menu
	void addSeparator() override;

	//! Returns text of the menu item.
	const wchar_t* getItemText(u32 idx) const override;

	//! Sets text of the menu item.
	void setItemText(u32 idx, const wchar_t* text) override;

	//! Returns if a menu item is enabled
	bool isItemEnabled(u32 idx) const override;

	//! Sets if the menu item should be enabled.
	void setItemEnabled(u32 idx, bool enabled) override;

	//! Returns if a menu item is checked
	bool isItemChecked(u32 idx) const override;

	//! Sets if the menu item should be checked.
	void setItemChecked(u32 idx, bool enabled) override;

	//! Removes a menu item
	void removeItem(u32 idx) override;

	//! Removes all menu items
	void removeAllItems() override;

	//! called if an event happened.
	bool OnEvent(const SEvent& event) override;

	//! draws the element and its children
	void draw() override;

	//! Returns the selected item in the menu
	s32 getSelectedItem() const override;

	//! Returns a pointer to the submenu of an item.
	//! \return Pointer to the submenu of an item.
	IGUIContextMenu* getSubMenu(u32 idx) const override;

	//! Sets the visible state of this element.
	void setVisible(bool visible) override;

	//! should the element change the checked status on clicking
	void setItemAutoChecking(u32 idx, bool autoChecking) override;

	//! does the element change the checked status on clicking
	bool getItemAutoChecking(u32 idx) const override;

	//! Returns command id of a menu item
	s32 getItemCommandId(u32 idx) const override;

	//! Sets the command id of a menu item
	void setItemCommandId(u32 idx, s32 id) override;

	//! Adds a sub menu from an element that already exists.
	void setSubMenu(u32 index, CGUICustomContextMenu* menu);

	//! When an eventparent is set it receives events instead of the usual parent element
	void setEventParent(IGUIElement *parent) override;

	//! Writes attributes of the element.
	void serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options) const override;

	//! Reads attributes of the element
	void deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options) override;

protected:

	void closeAllSubMenus();
	bool hasOpenSubMenu() const;

	struct SItem {
		core::stringw Text;
		bool IsCustom;
		bool IsSeparator;
		bool Enabled;
		bool Checked;
		bool AutoChecking;
		core::dimension2d<u32> Dim;
		s32 PosY;
		CGUICustomContextMenu* SubMenu;
		IGUIElement* Element;
		s32 CommandId;
	};

	virtual void recalculateSize();

	//! returns true, if an element was highlighted
	bool highlight(const core::vector2d<s32>& p, bool canOpenSubMenu);

	//! sends a click Returns:
	//! 0 if click went outside of the element,
	//! 1 if a valid button was clicked,
	//! 2 if a nonclickable element was clicked
	u32 sendClick(const core::vector2d<s32>& p);

	//! returns the item highlight-area
	virtual core::rect<s32> getHRect(const SItem& i, const core::rect<s32>& absolute) const;

	//! Gets drawing rect of Item
	virtual core::rect<s32> getRect(const SItem& i, const core::rect<s32>& absolute) const;

	irr::gui::IGUIScrollBar* scrOrizontal;
	irr::gui::IGUIScrollBar* scrVertical;

	core::rect<s32>* MaxRect;

	core::array<SItem> Items;
	core::vector2d<s32> Pos;
	IGUIElement* EventParent;
	IGUIFont *LastFont;
	ECONTEXT_MENU_CLOSE CloseHandling;
	s32 HighLighted;
	u32 ChangeTime;
	bool AllowFocus;
};


} // end namespace gui
} // end namespace irr

#endif // _IRR_COMPILE_WITH_GUI_

#endif // C_GUI_CUSTOM_CONTEXT_MENU_H

