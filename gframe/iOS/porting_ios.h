#ifndef PORTING_IOS_H
#define PORTING_IOS_H

#include <vector>
#include <string>
#include "../text_types.h"
#include <IEventReceiver.h> //irr::SEvent

namespace irr {
namespace video {
struct SExposedVideoData;
}
}

namespace porting {

extern const irr::video::SExposedVideoData* exposed_data;

void showErrorDialog(epro::stringview context, epro::stringview message);

void showComboBox(const std::vector<std::string>& parameters, int selected);

epro::path_string getWorkDir();

int changeWorkDir(const char* newdir);

int transformEvent(const irr::SEvent& event, bool& stopPropagation);

void dispatchQueuedMessages();

}

#endif //PORTING_IOS_H
