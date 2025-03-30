// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef C_GUI_CUSTOM_MENU_H
#define C_GUI_CUSTOM_MENU_H

#include <IrrCompileConfig.h>
#ifdef _IRR_COMPILE_WITH_GUI_

#include "CGUICustomContextMenu.h"

namespace irr {
namespace gui {

//! GUI menu interface.
class CGUICustomMenu final : public CGUICustomContextMenu {
public:

	//! constructor
	CGUICustomMenu(IGUIEnvironment* environment, IGUIElement* parent, s32 id, core::rect<s32> rectangle);

	static IGUIContextMenu* addCustomMenu(IGUIEnvironment* environment, IGUIElement* parent = nullptr, s32 id = -1, core::rect<s32>* rectangle = nullptr);

	//! draws the element and its children
	void draw() override;

	//! called if an event happened.
	bool OnEvent(const SEvent& event) override;

	//! Updates the absolute position.
	void updateAbsolutePosition() override;

protected:

	void recalculateSize() override;

	//! returns the item highlight-area
	core::rect<s32> getHRect(const SItem& i, const core::rect<s32>& absolute) const override;

	//! Gets drawing rect of Item
	core::rect<s32> getRect(const SItem& i, const core::rect<s32>& absolute) const override;
};

} // end namespace gui
} // end namespace irr


#endif // _IRR_COMPILE_WITH_GUI_
#endif //  C_GUI_CUSTOM_MENU_H

