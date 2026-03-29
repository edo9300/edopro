#ifndef SETTINGS_WINDOW_H
#define SETTINGS_WINDOW_H

namespace irr {
namespace gui {
class CGUIWindowedTabControl;
class IGUIWindow;
class IGUICheckBox;
class IGUIStaticText;
class IGUIScrollBar;
class IGUIComboBox;
class IGUIButton;
class IGUIEditBox;
class IGUIEnvironment;
class IGUITab;
class Panel;
}
}

namespace ygo {

struct SettingsPane {
	irr::gui::IGUICheckBox* chkIgnoreOpponents;
	irr::gui::IGUICheckBox* chkIgnoreSpectators;
	irr::gui::IGUICheckBox* chkQuickAnimation;
	irr::gui::IGUICheckBox* chkTopdown;
	irr::gui::IGUICheckBox* chkKeepFieldRatio;
	irr::gui::IGUICheckBox* chkAlternativePhaseLayout;
	irr::gui::IGUICheckBox* chkHideChainButtons;
	irr::gui::IGUICheckBox* chkAutoChainOrder;
	// audio
	irr::gui::IGUICheckBox* chkEnableSound;
	irr::gui::IGUIStaticText* stSoundVolume;
	irr::gui::IGUIScrollBar* scrSoundVolume;
	irr::gui::IGUICheckBox* chkEnableMusic;
	irr::gui::IGUIStaticText* stMusicVolume;
	irr::gui::IGUIScrollBar* scrMusicVolume;
	irr::gui::IGUIStaticText* stNoAudioBackend;

	irr::gui::IGUICheckBox* chkNoChainDelay;

	irr::gui::IGUICheckBox* chkIgnoreDeckContents;

	irr::gui::IGUICheckBox* chkEnableGenesysDeckBuilding;

	void DisableAudio();
};

struct SettingsWindow {
	irr::gui::CGUIWindowedTabControl* tabcontrolwindow;
	irr::gui::IGUIWindow* window;
	struct SettingsTab {
		irr::gui::IGUITab* tab{};
		irr::gui::Panel* panel{};
		void construct(irr::gui::IGUIEnvironment* env, irr::gui::CGUIWindowedTabControl* tabControl, const wchar_t* name);
	};
	SettingsTab client;
	irr::gui::IGUICheckBox* chkShowFPS;
	irr::gui::IGUICheckBox* chkHidePasscodeScope;
	irr::gui::IGUICheckBox* chkShowScopeLabel;
	irr::gui::IGUICheckBox* chkHideSetname;
	irr::gui::IGUIStaticText* stCurrentSkin;
	irr::gui::IGUIComboBox* cbCurrentSkin;
	irr::gui::IGUIButton* btnReloadSkin;
	irr::gui::IGUIStaticText* stCurrentLocale;
	irr::gui::IGUIComboBox* cbCurrentLocale;
	irr::gui::IGUIStaticText* stDpiScale;
	irr::gui::IGUIEditBox* ebDpiScale;
	irr::gui::IGUIButton* btnRestart;
	irr::gui::IGUICheckBox* chkUpdates;
	irr::gui::IGUICheckBox* chkFilterBot;
	irr::gui::IGUICheckBox* chkHideHandsInReplays;
	irr::gui::IGUICheckBox* chkConfirmDeckClear;
	irr::gui::IGUICheckBox* chkIgnoreDeckContents;
	irr::gui::IGUICheckBox* chkAddCardNamesInDeckList;
	irr::gui::IGUICheckBox* chkEnableGenesys;

	SettingsTab duel;
	irr::gui::IGUICheckBox* chkIgnoreOpponents;
	irr::gui::IGUICheckBox* chkIgnoreSpectators;
	irr::gui::IGUICheckBox* chkQuickAnimation;
	irr::gui::IGUICheckBox* chkTopdown;
	irr::gui::IGUICheckBox* chkKeepFieldRatio;
	irr::gui::IGUICheckBox* chkKeepCardRatio;
	irr::gui::IGUICheckBox* chkAlternativePhaseLayout;
	irr::gui::IGUICheckBox* chkHideChainButtons;
	irr::gui::IGUICheckBox* chkAutoChainOrder;
	irr::gui::IGUICheckBox* chkMAutoPos;
	irr::gui::IGUICheckBox* chkSTAutoPos;
	irr::gui::IGUICheckBox* chkRandomPos;
	irr::gui::IGUICheckBox* chkNoChainDelay;
	irr::gui::IGUICheckBox* chkAutoRPS;

	SettingsTab sound;
	irr::gui::IGUICheckBox* chkEnableSound;
	irr::gui::IGUIStaticText* stSoundVolume;
	irr::gui::IGUIScrollBar* scrSoundVolume;
	irr::gui::IGUICheckBox* chkEnableMusic;
	irr::gui::IGUIStaticText* stMusicVolume;
	irr::gui::IGUIScrollBar* scrMusicVolume;
	irr::gui::IGUICheckBox* chkLoopMusic;
	irr::gui::IGUIStaticText* stNoAudioBackend;
	irr::gui::IGUIStaticText* stAudioBackend;
	irr::gui::IGUIComboBox* cbAudioBackend;

	SettingsTab graphics;
	irr::gui::IGUICheckBox* chkScaleBackground;
	irr::gui::IGUICheckBox* chkAccurateBackgroundResize;
	irr::gui::IGUICheckBox* chkDrawFieldSpells;
	irr::gui::IGUIStaticText* stAntiAlias;
	irr::gui::IGUIEditBox* ebAntiAlias;
	irr::gui::IGUIStaticText* stVSync;
	irr::gui::IGUIComboBox* cbVSync;
	irr::gui::IGUIStaticText* stFPSCap;
	irr::gui::IGUIEditBox* ebFPSCap;
	irr::gui::IGUIButton* btnFPSCap;
	irr::gui::IGUIStaticText* stVideoDriver;
	irr::gui::IGUIComboBox* cbVideoDriver;
	irr::gui::IGUICheckBox* chkDottedLines;

	SettingsTab system;
	irr::gui::IGUICheckBox* chkFullscreen;
	irr::gui::IGUICheckBox* chkShowConsole;
	irr::gui::IGUIStaticText* stCoreLogOutput;
	irr::gui::IGUIComboBox* cbCoreLogOutput;
	irr::gui::IGUIStaticText* stMaxImagesPerFrame;
	irr::gui::IGUIEditBox* ebMaxImagesPerFrame;
	irr::gui::IGUIStaticText* stImageLoadThreads;
	irr::gui::IGUIEditBox* ebImageLoadThreads;
	irr::gui::IGUIStaticText* stImageDownloadThreads;
	irr::gui::IGUIEditBox* ebImageDownloadThreads;
	irr::gui::IGUICheckBox* chkDiscordIntegration;
	irr::gui::IGUICheckBox* chkLogDownloadErrors;
	irr::gui::IGUICheckBox* chkIntegratedGPU;
	irr::gui::IGUICheckBox* chkNativeMouse;
	irr::gui::IGUICheckBox* chkNativeKeyboard;

	void DisableAudio();
};

}

#endif
