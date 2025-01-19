#ifndef PORTING_H
#define PORTING_H
#include "compiler_features.h"
#if EDOPRO_ANDROID
#include "Android/porting_android.h"
#elif EDOPRO_IOS
#include "iOS/porting_ios.h"
#elif EDOPRO_PSVITA
#include "PSVita/porting_psvita.h"
#endif
#endif
