// Copyright (C) 2022 Edoardo Lolletti
// SPDX-License-Identifier: AGPL-3.0-or-later
// Refer to the COPYING file included.

#ifndef C_GUI_WINDOWED_TAB_CONTROL_H_INCLUDED
#define C_GUI_WINDOWED_TAB_CONTROL_H_INCLUDED

#include <IrrCompileConfig.h>

#include <IGUITabControl.h>
#include <vector2d.h>
#include <rect.h>

namespace irr {
namespace gui {
class IGUIWindow;
class IGUITabControl;
class IGUIEnvironment;

class CGUIWindowedTabControl {
public:
	static CGUIWindowedTabControl* addCGUIWindowedTabControl(IGUIEnvironment* environment, const core::rect<s32>& rectangle, const wchar_t* text);

	void setRelativePosition(const core::rect<s32>& target_rect);

	auto addTab(const wchar_t* caption) {
		return tabControl->addTab(caption);
	}

	auto getWindow() const { return window; };

private:
	CGUIWindowedTabControl(IGUIEnvironment* environment, const core::rect<s32>& rectangle, const wchar_t* text);
	core::rect<s32> calculateWindowTargetRect(core::rect<s32> target_rect) const;
	IGUIWindow* window;
	IGUITabControl* tabControl;
	s32 tabHeight;
	core::vector2di windowBorders;
};


} // end namespace gui
} // end namespace irr

#endif //C_GUI_WINDOWED_TAB_CONTROL_H_INCLUDED

