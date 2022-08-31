// Copyright (C) 2021 Edoardo Lolletti

#ifndef __C_OS_IOS_OPERATOR_H_INCLUDED__
#define __C_OS_IOS_OPERATOR_H_INCLUDED__

#include <IOSOperator.h>
#include <string>

namespace irr {

//! The Operating system operator provides operation system specific methods and information.
class COSiOSOperator : public IOSOperator {
public:

	COSiOSOperator();

	//! returns the current operation system version as string.
	virtual const core::stringc& getOperatingSystemVersion() const _IRR_OVERRIDE_;

	//! copies text to the clipboard
	virtual void copyToClipboard(const wchar_t* text) const _IRR_OVERRIDE_;

	//! gets text from the clipboard
	//! \return Returns 0 if no string is in there.
	virtual const wchar_t* getTextFromClipboard() const _IRR_OVERRIDE_;

	//! gets the processor speed in megahertz
	//! \param Mhz:
	//! \return Returns true if successful, false if not
	virtual bool getProcessorSpeedMHz(u32* MHz) const _IRR_OVERRIDE_;

	//! gets the total and available system RAM in kB
	//! \param Total: will contain the total system memory
	//! \param Avail: will contain the available memory
	//! \return Returns true if successful, false if not
	virtual bool getSystemMemory(u32* Total, u32* Avail) const _IRR_OVERRIDE_;

private:

	core::stringc OperatingSystem;
	
	mutable std::wstring ClipboardString;

};

} // __C_OS_IOS_OPERATOR_H_INCLUDED__

#endif

