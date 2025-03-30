// Copyright (C) 2020 Edoardo Lolletti

#ifndef __ANDROID__
#error This file may only be compiled for android!
#endif

#include <sys/utsname.h>
#include <android/ndk-version.h>
#if __NDK_MAJOR__ >= 19
#include <android/api-level.h>
#else
#include <sys/system_properties.h>
#include <cstdlib>
static inline int android_get_device_api_level() {
  char value[92] = { 0 };
  if (__system_property_get("ro.build.version.sdk", value) < 1) return -1;
  int api_level = std::atoi(value);
  return (api_level > 0) ? api_level : -1;
}
#endif
#include "COSAndroidOperator.h"
#include "porting_android.h"
#include "../fmt.h"

namespace irr {

// constructor
COSAndroidOperator::COSAndroidOperator() {
#ifdef _DEBUG
	setDebugName("COSAndroidOperator");
#endif
	struct utsname LinuxInfo;
	uname(&LinuxInfo);
	const auto verstring = epro::format("Android version: {} {} {} {} {}",
									   android_get_device_api_level(), LinuxInfo.sysname, LinuxInfo.release, LinuxInfo.version, LinuxInfo.machine);
	OperatingSystem = { verstring.data(), (u32)verstring.size() };
}


//! returns the current operating system version as string.
const core::stringc& COSAndroidOperator::getOperatingSystemVersion() const {
	return OperatingSystem;
}


//! copies text to the clipboard
void COSAndroidOperator::copyToClipboard(const wchar_t* text) const {
	if(wcslen(text) == 0)
		return;
	porting::setTextToClipboard(text);
}


//! gets text from the clipboard
//! \return Returns 0 if no string is in there.
const wchar_t* COSAndroidOperator::getTextFromClipboard() const {
	return porting::getTextFromClipboard();
}


bool COSAndroidOperator::getProcessorSpeedMHz(u32* MHz) const {
	return false;
}

bool COSAndroidOperator::getSystemMemory(u32* Total, u32* Avail) const {
	return false;
}


} // end namespace

