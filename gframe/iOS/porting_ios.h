#ifndef PORTING_IOS_H
#define PORTING_IOS_H
#include "../text_types.h"

void EPRO_IOS_ShowErrorDialog(void* controller, const char* context, const char* message);
epro::path_string EPRO_IOS_GetWorkDir();
int EPRO_IOS_ChangeWorkDir(const char* newdir);
int EPRO_IOS_transformEvent(const void* event, int* stopPropagation, void* irrdevice);

#endif //PORTING_IOS_H
