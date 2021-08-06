// Copyright (C) 2021 Edoardo Lolletti

#import <UIKit/UIPasteboard.h>
#include <sys/utsname.h>
#include <fmt/format.h>
#include "../bufferio.h"
#include "COSiOSOperator.h"

namespace irr {

// constructor
COSiOSOperator::COSiOSOperator() {
#ifdef _DEBUG
	setDebugName("COSiOSOperator");
#endif
	auto version = [[NSProcessInfo processInfo] operatingSystemVersion];
	struct utsname name;
	uname(&name);
	const auto verstring = fmt::format("iOS version: {} {} {} {}",
									   version.majorVersion, version.minorVersion, version.patchVersion, name.version);
	OperatingSystem = { verstring.data(), (u32)verstring.size() };
	fmt::print("{}\n", OperatingSystem);
}


//! returns the current operating system version as string.
const core::stringc& COSiOSOperator::getOperatingSystemVersion() const {
	return OperatingSystem;
}


//! copies text to the clipboard
void COSiOSOperator::copyToClipboard(const wchar_t* wtext) const {
	auto wlen = wcslen(wtext);
	if(wlen == 0)
		return;
    @autoreleasepool {
        [UIPasteboard generalPasteboard].string = @(BufferIO::EncodeUTF8({wtext, wlen}).data());
    }
}


//! gets text from the clipboard
//! \return Returns 0 if no string is in there.
const wchar_t* COSiOSOperator::getTextFromClipboard() const {
    @autoreleasepool {
        UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
        NSString *string = pasteboard.string;
        if (string != nil)
			ClipboardString = BufferIO::DecodeUTF8(string.UTF8String);
        else
        	ClipboardString.clear();
    }
    return ClipboardString.data();
}


bool COSiOSOperator::getProcessorSpeedMHz(u32* MHz) const {
	return false;
}

bool COSiOSOperator::getSystemMemory(u32* Total, u32* Avail) const {
	return false;
}


} // end namespace
