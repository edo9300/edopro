// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __C_GUI_CHECKBOX_H_INCLUDED__
#define __C_GUI_CHECKBOX_H_INCLUDED__

#include <IrrCompileConfig.h>
#ifdef _IRR_COMPILE_WITH_GUI_

#include <IGUICheckBox.h>

namespace irr {
namespace gui {

class CGUICustomCheckBox final : public IGUICheckBox {
public:

	//! constructor
	CGUICustomCheckBox(bool checked, IGUIEnvironment* environment, IGUIElement* parent, s32 id, core::rect<s32> rectangle);

	static IGUICheckBox* addCustomCheckBox(bool checked, IGUIEnvironment* environment, core::rect<s32> rectangle, IGUIElement* parent = 0, s32 id = -1, const wchar_t* text = nullptr);

	//! set if box is checked
	void setChecked(bool checked) override;

	//! returns if box is checked
	bool isChecked() const override;

#if IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9

	//! Sets whether to draw the background
	void setDrawBackground(bool draw) override;

	//! Checks if background drawing is enabled
	/** \return true if background drawing is enabled, false otherwise */
	bool isDrawBackgroundEnabled() const override;

	//! Sets whether to draw the border
	void setDrawBorder(bool draw) override;

	//! Checks if border drawing is enabled
	/** \return true if border drawing is enabled, false otherwise */
	bool isDrawBorderEnabled() const override;
#endif

	//! called if an event happened.
	bool OnEvent(const SEvent& event) override;

	//! draws the element and its children
	void draw() override;

	//! Writes attributes of the element.
	void serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options) const override;

	//! Reads attributes of the element
	void deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options) override;

	void setColor(video::SColor color);

private:

	u32 checkTime;
	bool Pressed;
	bool Checked;
	video::SColor override_color;
	bool Border;
	bool Background;
};

} // end namespace gui
} // end namespace irr

#endif // __C_GUI_CHECKBOX_H_INCLUDED__

#endif // _IRR_COMPILE_WITH_GUI_
