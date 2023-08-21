// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

// 07.10.2005 - Multicolor-Listbox addet by A. Buschhueter (Acki)
//                                          A_Buschhueter@gmx.de

#ifndef __C_GUI_TABLE_BAR_H_INCLUDED__
#define __C_GUI_TABLE_BAR_H_INCLUDED__

#include <IrrCompileConfig.h>
#ifdef _IRR_COMPILE_WITH_GUI_

#include <IGUITable.h>
#include <irrArray.h>

namespace irr {
namespace gui {

class IGUIFont;
class IGUIScrollBar;

class CGUICustomTable final : public IGUITable {
public:
	//! constructor
	CGUICustomTable(IGUIEnvironment* environment, IGUIElement* parent,
					s32 id, const core::rect<s32>& rectangle, bool clip = true,
					bool drawBack = false, bool moveOverSelect = true);

	//! destructor
	~CGUICustomTable();

	static IGUITable* addCustomTable(IGUIEnvironment* environment, const core::rect<s32>& rectangle,
									 IGUIElement* parent = 0, s32 id = -1, bool drawBackground = false);

	//! Adds a column
	//! If columnIndex is outside the current range, do push new colum at the end
	void addColumn(const wchar_t* caption, s32 columnIndex = -1) override;

	//! remove a column from the table
	void removeColumn(u32 columnIndex) override;

	//! Returns the number of columns in the table control
	s32 getColumnCount() const override;

	//! Makes a column active. This will trigger an ordering process.
	/** \param idx: The id of the column to make active.
	\return True if successful. */
	bool setActiveColumn(s32 columnIndex, bool doOrder = false) override;

	//! Returns which header is currently active
	s32 getActiveColumn() const override;

	//! Returns the ordering used by the currently active column
	EGUI_ORDERING_MODE getActiveColumnOrdering() const override;

	//! set a column width
	void setColumnWidth(u32 columnIndex, u32 width) override;

	//! Get the width of a column
	u32 getColumnWidth(u32 columnIndex) const override;

	//! columns can be resized by drag 'n drop
	void setResizableColumns(bool resizable) override;

	//! can columns be resized by dran 'n drop?
	bool hasResizableColumns() const override;

	//! This tells the table control which ordering mode should be used when
	//! a column header is clicked.
	/** \param columnIndex: The index of the column header.
	\param state: If true, a EGET_TABLE_HEADER_CHANGED message will be sent and you can order the table data as you whish.*/
	//! \param mode: One of the modes defined in EGUI_COLUMN_ORDERING
	void setColumnOrdering(u32 columnIndex, EGUI_COLUMN_ORDERING mode) override;

	//! Returns which row is currently selected
	s32 getSelected() const override;

	//! set wich row is currently selected
	void setSelected(s32 index) override;

	//! Returns amount of rows in the tabcontrol
	s32 getRowCount() const override;

	//! adds a row to the table
	/** \param rowIndex: zero based index of rows. The row will be
		inserted at this position. If a row already exists
		there, it will be placed after it. If the row is larger
		than the actual number of rows by more than one, it
		won't be created. Note that if you create a row that is
		not at the end, there might be performance issues*/
	u32 addRow(u32 rowIndex) override;

	//! Remove a row from the table
	void removeRow(u32 rowIndex) override;

	//! clear the table rows, but keep the columns intact
	void clearRows() override;

	//! Swap two row positions. This is useful for a custom ordering algo.
	void swapRows(u32 rowIndexA, u32 rowIndexB) override;

	//! This tells the table to start ordering all the rows. You
	//! need to explicitly tell the table to reorder the rows when
	//! a new row is added or the cells data is changed. This makes
	//! the system more flexible and doesn't make you pay the cost
	//! of ordering when adding a lot of rows.
	//! \param columnIndex: When set to -1 the active column is used.
	void orderRows(s32 columnIndex = -1, EGUI_ORDERING_MODE mode = EGOM_NONE) override;


	//! Set the text of a cell
	void setCellText(u32 rowIndex, u32 columnIndex, const core::stringw& text) override;

	//! Set the text of a cell, and set a color of this cell.
	void setCellText(u32 rowIndex, u32 columnIndex, const core::stringw& text, video::SColor color) override;

	//! Set the data of a cell
	//! data will not be serialized.
	void setCellData(u32 rowIndex, u32 columnIndex, void *data) override;

	//! Set the color of a cell text
	void setCellColor(u32 rowIndex, u32 columnIndex, video::SColor color) override;

	//! Get the text of a cell
	const wchar_t* getCellText(u32 rowIndex, u32 columnIndex) const override;

	//! Get the data of a cell
	void* getCellData(u32 rowIndex, u32 columnIndex) const override;

	//! clears the table, deletes all items in the table
	void clear() override;

	//! called if an event happened.
	bool OnEvent(const SEvent &event) override;

	//! draws the element and its children
	void draw() override;

	//! Set flags, as defined in EGUI_TABLE_DRAW_FLAGS, which influence the layout
	void setDrawFlags(s32 flags) override;

	//! Get the flags, as defined in EGUI_TABLE_DRAW_FLAGS, which influence the layout
	s32 getDrawFlags() const override;

	void setColumnText(u32 columnIndex, const wchar_t* caption);

#if IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9
	//! Sets another skin independent font.
	void setOverrideFont(IGUIFont* font = 0) override;

	//! Gets the override font (if any)
	IGUIFont* getOverrideFont() const override;

	//! Get the font which is used right now for drawing
	IGUIFont* getActiveFont() const override;

	//! Get the height of items/rows
	s32 getItemHeight() const override;

	//! Access the vertical scrollbar
	IGUIScrollBar* getVerticalScrollBar() const override;

	//! Access the horizontal scrollbar
	IGUIScrollBar* getHorizontalScrollBar() const override;

	void setDrawBackground(bool) override {};

	bool isDrawBackgroundEnabled() const override { return false; };
#endif

	//! Writes attributes of the object.
	//! Implement this to expose the attributes of your scene node animator for
	//! scripting languages, editors, debuggers or xml serialization purposes.
	void serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options = 0) const override;

	//! Reads attributes of the object.
	//! Implement this to set the attributes of your scene node animator for
	//! scripting languages, editors, debuggers or xml deserialization purposes.
	void deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options = 0) override;

protected:
	void refreshControls();
	void checkScrollbars();

private:

	struct Cell {
		Cell() : IsOverrideColor(false), Data(0) {}
		core::stringw Text;
		core::stringw BrokenText;
		bool IsOverrideColor;
		video::SColor Color;
		void *Data;
	};

	struct Row {
		Row() {}
		core::array<Cell> Items;
	};

	struct Column {
		Column() : Width(0), OrderingMode(EGCO_NONE) {}
		core::stringw Name;
		u32 Width;
		EGUI_COLUMN_ORDERING OrderingMode;
	};

	void breakText(const core::stringw &text, core::stringw & brokenText, u32 cellWidth);
	void selectNew(s32 ypos, bool onlyHover = false);
	bool selectColumnHeader(s32 xpos, s32 ypos);
	bool dragColumnStart(s32 xpos, s32 ypos);
	bool dragColumnUpdate(s32 xpos);
	void recalculateHeights();
	void recalculateWidths();

	core::array< Column > Columns;
	core::array< Row > Rows;
	gui::IGUIFont* Font;
	gui::IGUIScrollBar* VerticalScrollBar;
	gui::IGUIScrollBar* HorizontalScrollBar;
	bool Clip;
	bool DrawBack;
	bool MoveOverSelect;
	bool Selecting;
	s32  CurrentResizedColumn;
	s32  ResizeStart;
	bool ResizableColumns;

	s32 ItemHeight;
	s32 TotalItemHeight;
	s32 TotalItemWidth;
	s32 Selected;
	s32 CellHeightPadding;
	s32 CellWidthPadding;
	s32 ActiveTab;
	EGUI_ORDERING_MODE CurrentOrdering;
	s32 DrawFlags;
	u32 selectTime;

	gui::IGUIFont* OverrideFont;
};

} // end namespace gui
} // end namespace irr

#endif // _IRR_COMPILE_WITH_GUI_

#endif

