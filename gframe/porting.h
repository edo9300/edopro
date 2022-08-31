#ifndef PORTING_H
#define PORTING_H
#include "config.h"
#ifdef __ANDROID__
#include "Android/porting_android.h"
#elif defined(EDOPRO_IOS)
#include "iOS/porting_ios.h"
#endif
#endif