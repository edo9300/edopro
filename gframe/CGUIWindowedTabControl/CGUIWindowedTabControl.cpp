// Copyright (C) 2022 Edoardo Lolletti
// SPDX-License-Identifier: AGPL-3.0-or-later
// Refer to the COPYING file included.

#include "CGUIWindowedTabControl.h"
#include <IGUIEnvironment.h>
#include <IGUIWindow.h>
#include "../CGUICustomTabControl/CGUICustomTabControl.h"
#include "../CGUIEnvironmentLinker.h"

namespace irr {
namespace gui {

CGUIWindowedTabControl* CGUIWindowedTabControl::addCGUIWindowedTabControl(IGUIEnvironment* environment, const core::rect<s32>& rectangle, const wchar_t* text) {
	auto* ret = new CGUIWindowedTabControl(environment, rectangle, text);
	EnvironmentLinker<CGUIWindowedTabControl>::tie(environment, ret);
	return ret;
}

CGUIWindowedTabControl::~CGUIWindowedTabControl() {
	tabControl->drop();
	window->drop();
}

static int getTabControlHeight(IGUIEnvironment* environment) {
	auto* tabControl = CGUICustomTabControl::addCustomTabControl(environment, core::rect<s32>{0, 0, 60, 60}, nullptr, false, false);
	auto* tab = tabControl->addTab(L"");
	auto tabHeight = tabControl->getAbsoluteClippingRect().getHeight() - tab->getAbsoluteClippingRect().getHeight();
	tabControl->removeTab(0);
	tabControl->remove();
	return tabHeight;
}

static core::vector2di getWindowBorderSizes(IGUIEnvironment* environment) {
	const core::rect<s32> test_rect{0, 0, 120, 120};
	auto* window = environment->addWindow(test_rect);
	const auto client_rect = window->getClientRect();
	core::vector2di borders;
	borders.X = test_rect.getWidth() - client_rect.getWidth();
	borders.Y = test_rect.getHeight() - client_rect.getHeight();
	window->remove();
	return borders;
}

core::rect<s32> CGUIWindowedTabControl::calculateWindowTargetRect(core::rect<s32> target_rect) const {
	target_rect.UpperLeftCorner.Y -= tabHeight;
	target_rect.LowerRightCorner.Y += windowBorders.Y;
	target_rect.UpperLeftCorner.X -= windowBorders.X / 2;
	target_rect.LowerRightCorner.X += (windowBorders.X / 2) + (windowBorders.X & 1);
	return target_rect;
}

CGUIWindowedTabControl::CGUIWindowedTabControl(IGUIEnvironment* environment, const core::rect<s32>& rectangle, const wchar_t* text):
	window(nullptr), tabControl(nullptr), tabHeight(getTabControlHeight(environment)),
	windowBorders(getWindowBorderSizes(environment)), clientRect{ {},rectangle.getSize() } {
	window = environment->addWindow(calculateWindowTargetRect(rectangle), false, text);
	window->grab();
	tabControl = CGUICustomTabControl::addCustomTabControl(environment, window->getClientRect(), window, false, false);
	tabControl->grab();
}

void CGUIWindowedTabControl::setRelativePosition(const core::rect<s32>& target_rect) {
	window->setRelativePosition(calculateWindowTargetRect(target_rect));
	tabControl->setRelativePosition(window->getClientRect());
	clientRect = { {},target_rect.getSize() };
}

} // end namespace irr
} // end namespace gui

