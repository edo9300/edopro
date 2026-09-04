subdir('lzma')

ygopro_sources = [
	'CGUICustomCheckBox/CGUICustomCheckBox.cpp',
	'CGUICustomContextMenu/CGUICustomContextMenu.cpp',
	'CGUICustomComboBox/CGUICustomComboBox.cpp',
	'CGUICustomContextMenu/CGUICustomMenu.cpp',
	'CGUICustomTabControl/CGUICustomTabControl.cpp',
	'CGUICustomTable/CGUICustomTable.cpp',
	'CGUICustomText/CGUICustomText.cpp',
	'CGUIFileSelectListBox/CGUIFileSelectListBox.cpp',
	'CGUIImageButton/CGUIImageButton.cpp',
	'CGUISkinSystem/CConfigMap.cpp',
	'CGUISkinSystem/CGUIProgressBar.cpp',
	'CGUISkinSystem/CGUISkinSystem.cpp',
	'CGUISkinSystem/CImageGUISkin.cpp',
	'CGUITTFont/CGUITTFont.cpp',
	'CGUIWindowedTabControl/CGUIWindowedTabControl.cpp',
	'CProgressBar/CProgressBar.cpp',
	'CXMLRegistry/CXMLNode.cpp',
	'CXMLRegistry/CXMLRegistry.cpp',
	'MD5/md5.c',
	'ResizeablePanel/ResizeablePanel.cpp',
	'address.cpp',
	'client_card.cpp',
	'client_field.cpp',
	'client_updater.cpp',
	'core_utils.cpp',
	'crypto.cpp',
	'custom_skin_enum.cpp',
	'data_handler.cpp',
	'data_manager.cpp',
	'deck_con.cpp',
	'deck_manager.cpp',
	'discord_wrapper.cpp',
	'dll.cpp',
	'dllinterface.cpp',
	'drawing.cpp',
	'duelclient.cpp',
	'edopro_main.cpp',
	'event_handler.cpp',
	'game_config.cpp',
	'game.cpp',
	'generic_duel.cpp',
	'gframe.cpp',
	'image_downloader.cpp',
	'image_manager.cpp',
	'ireadfile_sqlite.cpp',
	'joystick_wrapper.cpp',
	'logging.cpp',
	'materials.cpp',
	'menu_handler.cpp',
	'netserver.cpp',
	'old_replay_mode.cpp',
	'replay.cpp',
	'replay_mode.cpp',
	'repo_cloner.cpp',
	'repo_manager.cpp',
	'server_lobby.cpp',
	'settings_window.cpp',
	'single_mode.cpp',
	'sound_manager.cpp',
	'utils.cpp',
	'utils_gui.cpp',
	'windbot.cpp',
	'windbot_panel.cpp',
]

if sounds.length() > 0
	ygopro_sources += 'SoundBackends/sound_threaded_backend.cpp'
endif

compile_args = []

if sounds.contains('sfml')
	ygopro_sources += 'SoundBackends/sfml/sound_sfml.cpp'
	compile_args += '-DYGOPRO_USE_SFML'
endif

if sounds.contains('irrklang')
	ygopro_sources += 'SoundBackends/irrklang/irrklang_dynamic_loader.cpp'
	ygopro_sources += 'SoundBackends/irrklang/sound_irrklang.cpp'
	compile_args += '-DYGOPRO_USE_IRRKLANG'
endif

if sounds.contains('miniaudio')
	ygopro_sources += 'SoundBackends/miniaudio/sound_miniaudio.cpp'
	compile_args += '-DYGOPRO_USE_MINIAUDIO'
	if host_machine.system() == 'darwin'
		compile_args += '-DMA_NO_RUNTIME_LINKING'
	endif
endif

if sounds.contains('sdl-mixer')
	ygopro_sources += 'SoundBackends/sdlmixer/sound_sdlmixer.cpp'
	compile_args += '-DYGOPRO_USE_SDL_MIXER'
elif sounds.contains('sdl3-mixer')
	ygopro_sources += 'SoundBackends/sdlmixer3/sound_sdlmixer3.cpp'
	compile_args += '-DYGOPRO_USE_SDL_MIXER3'
endif

cpp = meson.get_compiler('cpp')
if cpp.get_id() != 'msvc'
	if cpp.has_argument('-Wno-unknown-warning-option')
		compile_args += ['-Wno-unknown-warning-option']
	endif
	if cpp.has_argument('-Wno-implicit-const-int-float-conversion')
		compile_args += ['-Wno-implicit-const-int-float-conversion']
	endif
	compile_args += [
		'-Wno-unused-parameter',
		'-Wno-unknown-pragmas',
		'-Wno-ignored-qualifiers',
		'-Wno-missing-field-initializers',
		'-Wno-missing-braces',
		'-Wno-invalid-utf8',
	]
endif

if host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
	compile_args += ['-DNOMINMAX']
	ygopro_sources += import('windows').compile_resources(
		'ygopro.rc',
		args: ['-D__MINGW32__'],
		depend_files: ['ygopro.ico', 'ygopro.exe.manifest'],
	)
endif

if host_machine.subsystem() == 'macos'
	add_languages('objc', 'objcpp', native: false)
	ygopro_sources += [
		'OSX/porting_osx.mm',
		'discord_register_url_osx.m',
	]
elif host_machine.subsystem().startswith('ios')
	add_languages('objc')
	ygopro_sources += [
		'iOS/COSiOSOperator.mm',
		'iOS/porting_ios.mm',
	]
endif

if get_option('pics') != ''
	compile_args += '-DDEFAULT_PIC_URL="' + get_option('pics') + '"'
endif
if get_option('fields') != ''
	compile_args += '-DDEFAULT_FIELD_URL="' + get_option('fields') + '"'
endif
if get_option('covers') != ''
	compile_args += '-DDEFAULT_COVER_URL="' + get_option('covers') + '"'
endif
if get_option('discord') != ''
	compile_args += '-DDISCORD_APP_ID="' + get_option('discord') + '"'
endif
if get_option('update-url') != ''
	compile_args += '-DUPDATE_URL="' + get_option('update-url') + '"'
endif

edopro_deps = [
	ocgcore,
	irrlicht_dep,
	freetype2_dep,
	libgit2_dep,
	fmt_dep,
	libevent_dep,
	nlohmann_dep,
	curl_dep,
	sqlite3_dep,
	lzma_dep,
	sound_deps,
]

if host_machine.system() == 'windows'
	edopro_deps += cpp.find_library('ws2_32')
elif host_machine.system() == 'darwin'
	edopro_deps += dependency('appleframeworks', modules: 'Foundation')
	if host_machine.subsystem() == 'macos'
		edopro_deps += dependency('appleframeworks', modules: 'AppKit')
	endif
endif

ygopro = executable(
	'ygopro',
	include_directories : [
		'./',
	],
	cpp_args : compile_args + irrlicht_defines,
    dependencies: edopro_deps,
    sources: ygopro_sources,
)
