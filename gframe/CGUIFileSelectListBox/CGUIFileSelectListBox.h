// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __C_GUI_FILE_SELECT_LIST_BOX_H_INCLUDED__
#define __C_GUI_FILE_SELECT_LIST_BOX_H_INCLUDED__

#include <IrrCompileConfig.h>
#ifdef _IRR_COMPILE_WITH_GUI_

#include <vector>
#include <string>
#include <functional>
#include <IGUIListBox.h>
#include "../utils.h"

namespace irr {
namespace gui {

class IGUIFont;
class IGUIScrollBar;

class CGUIFileSelectListBox final : public IGUIListBox {
public:
	//! constructor
	CGUIFileSelectListBox(IGUIEnvironment* environment, IGUIElement* parent,
						  s32 id, core::rect<s32> rectangle, bool clip = true,
						  bool drawBack = false, bool moveOverSelect = false);

	static CGUIFileSelectListBox* addFileSelectListBox(IGUIEnvironment* environment, IGUIElement* parent,
													   s32 id, core::rect<s32> rectangle, bool clip = true,
													   bool drawBack = false, bool moveOverSelect = false);

	//! destructor
	~CGUIFileSelectListBox() override;

	//! returns amount of list items
	u32 getItemCount() const override;

	//! returns string of a list item. the id may be a value from 0 to itemCount-1

	const wchar_t* getListItem(u32 id) const override;

	const wchar_t* getListItem(u32 id, bool relativepath) const;

	//! adds an list item, returns id of item
	u32 addItem(const wchar_t* text) override;

	//! clears the list
	void clear() override;

	//! returns id of selected item. returns -1 if no item is selected.
	s32 getSelected() const override;

	//! sets the selected item. Set this to -1 if no item should be selected
	void setSelected(s32 id) override;

	//! sets the selected item. Set this to -1 if no item should be selected
	void setSelected(const wchar_t *item) override;

	//! called if an event happened.
	bool OnEvent(const SEvent& event) override;

	//! draws the element and its children
	void draw() override;

	//! adds an list item with an icon
	//! \param text Text of list entry
	//! \param icon Sprite index of the Icon within the current sprite bank. Set it to -1 if you want no icon
	//! \return
	//! returns the id of the new created item
	u32 addItem(const wchar_t* text, s32 icon) override;

	//! Returns the icon of an item
	s32 getIcon(u32 id) const override;

	//! removes an item from the list
	void removeItem(u32 id) override;

	//! get the the id of the item at the given absolute coordinates
	s32 getItemAt(s32 xpos, s32 ypos) const override;

	//! Sets the sprite bank which should be used to draw list icons. This font is set to the sprite bank of
	//! the built-in-font by default. A sprite can be displayed in front of every list item.
	//! An icon is an index within the icon sprite bank. Several default icons are available in the
	//! skin through getIcon
	void setSpriteBank(IGUISpriteBank* bank) override;

	//! set whether the listbox should scroll to newly selected items
	void setAutoScrollEnabled(bool scroll) override;

	//! returns true if automatic scrolling is enabled, false if not.
	bool isAutoScrollEnabled() const override;

	//! Update the position and size of the listbox, and update the scrollbar
	void updateAbsolutePosition() override;

	//! Writes attributes of the element.
	void serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options) const override;

	//! Reads attributes of the element
	void deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options) override;

	//! set all item colors at given index to color
	void setItemOverrideColor(u32 index, video::SColor color) override;

	//! set all item colors of specified type at given index to color
	void setItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType, video::SColor color) override;

	//! clear all item colors at index
	void clearItemOverrideColor(u32 index) override;

	//! clear item color at index for given colortype
	void clearItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType) override;

	//! has the item at index its color overwritten?
	bool hasItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType) const override;

	//! return the overwrite color at given item index.
	video::SColor getItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType) const override;

	//! return the default color which is used for the given colorType
	video::SColor getItemDefaultColor(EGUI_LISTBOX_COLOR colorType) const override;

	//! set the item at the given index
	void setItem(u32 index, const wchar_t* text, s32 icon) override;

	//! Insert the item at the given index
	//! Return the index on success or -1 on failure.
	s32 insertItem(u32 index, const wchar_t* text, s32 icon) override;

	//! Swap the items at the given indices
	void swapItems(u32 index1, u32 index2) override;

	//! set global itemHeight
	void setItemHeight(s32 height) override;

	//! Sets whether to draw the background
	void setDrawBackground(bool draw) override;

#if IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9
	IGUIScrollBar* getVerticalScrollBar() const override;
#endif

	void refreshList();

	void resetPath();

	void setWorkingPath(const std::wstring& newDirectory, bool setAsRoot = false);

	using callback = bool(std::wstring, bool);

	void addFilterFunction(callback* function);

	void addFilteredExtensions(std::vector<std::wstring> extensions);

	bool defaultFilter(std::wstring name, bool is_directory);

	void nativeDirectory(bool native_directory);

	bool isDirectory(u32 index);

	void enterDirectory(u32 index);


private:

	struct ListItem {
		ListItem() : icon(-1) {
		}

		std::wstring reltext;
		bool isDirectory;
		std::wstring text;
		s32 icon;

		// A multicolor extension
		struct ListItemOverrideColor {
			ListItemOverrideColor() : Use(false) {}
			bool Use;
			video::SColor Color;
		};
		ListItemOverrideColor OverrideColors[EGUI_LBC_COUNT];

		bool operator ==(const struct ListItem& other) const {
			if(isDirectory != other.isDirectory)
				return false;

			return ygo::Utils::EqualIgnoreCase(reltext, other.reltext);
		}

		bool operator <(const struct ListItem& other) const {
			if(isDirectory != other.isDirectory)
				return isDirectory;

			return ygo::Utils::CompareIgnoreCase(reltext, other.reltext);
		}
	};

	void recalculateItemHeight();
	void selectNew(s32 ypos, bool onlyHover = false);
	void recalculateScrollPos();

	// extracted that function to avoid copy&paste code
	void recalculateItemWidth(s32 icon);

	// get labels used for serialization
	bool getSerializationLabels(EGUI_LISTBOX_COLOR colorType, core::stringc & useColorLabel, core::stringc & colorLabel) const;

	void LoadFolderContents();

	std::wstring basePath;
	std::wstring prevRelPath;
	std::wstring curRelPath;
	callback* filter;
	//std::function<bool(std::wstring, bool, void*)> filter;
	std::vector<std::wstring> filtered_extensions;

	std::vector<ListItem> Items;
	s32 Selected;
	s32 ItemHeight;
	s32 ItemHeightOverride;
	s32 TotalItemHeight;
	s32 ItemsIconWidth;
	gui::IGUIFont* Font;
	gui::IGUISpriteBank* IconBank;
	gui::IGUIScrollBar* ScrollBar;
	u32 selectTime;
	u32 LastKeyTime;
	std::wstring KeyBuffer;
	bool Selecting;
	int TotalFolders;
	bool BaseIsRoot;
	bool DrawBack;
	bool MoveOverSelect;
	bool AutoScroll;
	bool HighlightWhenNotFocused;
	bool NativeDirectoryHandling;
};


} // end namespace gui
} // end namespace irr

#endif // _IRR_COMPILE_WITH_GUI_

#endif
