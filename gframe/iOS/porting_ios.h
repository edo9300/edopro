#ifndef PORTING_IOS_H
#define PORTING_IOS_H
#include <IEventReceiver.h> //irr::SEvent
#include "../text_types.h"

namespace irr {
namespace video {
class SExposedVideoData;
}
}

namespace porting {

void showErrorDialog(epro::stringview context, epro::stringview message);
void showComboBox(const std::vector<std::string>& parameters, int selected);
epro::path_string getWorkDir();
int changeWorkDir(const char* newdir);
int transformEvent(const irr::SEvent& event, bool& stopPropagation);
void dispatchQueuedMessages();

extern const irr::video::SExposedVideoData* exposed_data;

}

#endif //PORTING_IOS_H
