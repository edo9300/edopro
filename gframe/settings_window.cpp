#include "settings_window.h"
#include <IGUIEnvironment.h>
#include <IGUICheckBox.h>
#include <IGUIScrollBar.h>
#include <IGUIStaticText.h>
#include "ResizeablePanel/ResizeablePanel.h"
#include "CGUIWindowedTabControl/CGUIWindowedTabControl.h"

namespace ygo {

void SettingsPane::DisableAudio() {
	chkEnableSound->setVisible(false);
	stSoundVolume->setVisible(false);
	scrSoundVolume->setVisible(false);
	chkEnableMusic->setVisible(false);
	stMusicVolume->setVisible(false);
	scrMusicVolume->setVisible(false);
	stNoAudioBackend->setVisible(true);
}

void SettingsWindow::DisableAudio() {
	chkEnableSound->setVisible(false);
	stSoundVolume->setVisible(false);
	scrSoundVolume->setVisible(false);
	chkEnableMusic->setVisible(false);
	stMusicVolume->setVisible(false);
	scrMusicVolume->setVisible(false);
	chkLoopMusic->setVisible(false);
	stNoAudioBackend->setVisible(true);
}

void SettingsWindow::SettingsTab::construct(irr::gui::IGUIEnvironment* env, irr::gui::CGUIWindowedTabControl* tabControl, const wchar_t* name) {
	tab = tabControl->addTab(name);
	panel = irr::gui::Panel::addPanel(env, tab, -1, tabControl->getClientRect(), true, false);
}
}
