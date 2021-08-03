#ifndef PORTING_IOS_H
#define PORTING_IOS_H
#include <SExposedVideoData.h>
#include "../text_types.h"

void EPRO_IOS_ShowErrorDialog(const char* context, const char* message);
epro::path_string EPRO_IOS_GetWorkDir();
int EPRO_IOS_ChangeWorkDir(const char* newdir);
int EPRO_IOS_transformEvent(const void* event, int* stopPropagation, void* irrdevice);
void EPRO_IOS_dispatchQueuedMessages();

extern const irr::video::SExposedVideoData* ios_exposed_data;

#endif //PORTING_IOS_H
