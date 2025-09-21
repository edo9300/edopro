#ifndef PORTING_ANDROID_H
#define PORTING_ANDROID_H

#include <vector>
#include <string>
#include "../text_types.h"
#include "../address.h"

namespace irr {
class IrrlichtDevice;
struct SEvent;
}

struct android_app;

namespace porting {
/** java app **/
extern android_app* app_global;

extern std::string internal_storage;

bool transformEvent(const irr::SEvent& event, bool& stopPropagation);

void showComboBox(const std::vector<std::string>& parameters, int selected);

std::vector<epro::Address> getLocalIP();

void launchWindbot(epro::path_stringview args);

void addWindbotDatabase(epro::path_stringview args);

void installUpdate(epro::path_stringview args);

void openUrl(epro::path_stringview url);

void openFile(epro::path_stringview file);

void shareFile(epro::path_stringview file);

void setTextToClipboard(epro::wstringview text);

const wchar_t* getTextFromClipboard();

void dispatchQueuedMessages();

void showErrorDialog(epro::stringview context, epro::stringview message);

bool deleteFileUri(epro::path_stringview fileUri);

int openFdFromUri(epro::path_stringview fileUri, epro::path_stringview mode);

std::vector<epro::path_string> listFilesInFolder(epro::path_stringview folderUri);

static inline bool pathIsUri(epro::path_stringview path) {
	return starts_with(path, EPRO_TEXT("content://"));
}
}

#endif //PORTING_ANDROID_H
