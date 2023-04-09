#ifndef PORTING_H
#define PORTING_H
#include "config.h"
#if EDOPRO_ANDROID
#include "Android/porting_android.h"
#elif EDOPRO_IOS
#include "iOS/porting_ios.h"
#endif
#endif