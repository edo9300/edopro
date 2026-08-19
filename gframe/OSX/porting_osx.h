#ifndef PORTING_OSX_H
#define PORTING_OSX_H

#include <string>

namespace porting {

void setupMenuBar(void (*fullscreenCallback)());

void toggleFullScreen();

std::string getWindowRect(void* window);
void setWindowRect(void* window, const char* rect_string);

void nameThread(const char* name);

}

#endif /* PORTING_OSX_H */
