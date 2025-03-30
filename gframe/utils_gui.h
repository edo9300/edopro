#ifndef UTILS_GUI_H
#define UTILS_GUI_H

#include "text_types.h"
#include <memory>

namespace irr {
class IrrlichtDevice;
namespace video {
class IVideoDriver;
}
namespace gui {
class IGUIElement;
class IGUICheckBox;
}
}

namespace ygo {

struct GameConfig;

namespace GUIUtils {

std::shared_ptr<irr::IrrlichtDevice> CreateDevice(GameConfig* configs);
void ChangeCursor(std::shared_ptr<irr::IrrlichtDevice>& device, /*irr::gui::ECURSOR_ICON*/ int icon);
bool TakeScreenshot(std::shared_ptr<irr::IrrlichtDevice>& device);
void ToggleFullscreen(std::shared_ptr<irr::IrrlichtDevice>& device, bool& fullscreen);
void ShowErrorWindow(epro::stringview context, epro::stringview message);
void ToggleSwapInterval(irr::video::IVideoDriver* driver, int interval);
std::string SerializeWindowPosition(std::shared_ptr<irr::IrrlichtDevice>& device);
void TriggerEvent(std::shared_ptr<irr::IrrlichtDevice>& device, irr::gui::IGUIElement* target, /*irr::gui::EGUI_EVENT_TYPE*/ int type);
void ClickButton(std::shared_ptr<irr::IrrlichtDevice>& device, irr::gui::IGUIElement* btn);
void SetCheckbox(std::shared_ptr<irr::IrrlichtDevice>& device, irr::gui::IGUICheckBox* chk, bool state);

}

}

#endif
