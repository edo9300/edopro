project('edopro', ['c', 'cpp'],
	license : 'AGPL-3.0-or-later',
	default_options : ['cpp_std=gnu++17', 'warning_level=3', 'cpp_rtti=false', 'backend_startup_project=ygopro'],
	version : '41.0.2',
)

ocgcore_proj = subproject('ocgcore',  default_options: ['default_library=static'])
ocgcore = ocgcore_proj.get_variable('ocgcore_dep')

irrlicht_dep = dependency('irrlicht', required : false)

if not irrlicht_dep.found()
	subdir('irrlicht')
endif

freetype2_dep = dependency('freetype2')
libgit2_dep = dependency('libgit2')
fmt_dep = dependency('fmt')
if host_machine.system() == 'windows'
	libevent_dep = dependency('libevent')
else
	libevent_dep = dependency('libevent_pthreads')
endif
nlohmann_dep = dependency('nlohmann_json')
curl_dep = dependency('libcurl')
sqlite3_dep = dependency('sqlite3')

sounds = get_option('sound')
sound_deps = []
if sounds.contains('sfml')
	subdir('sfAudio')
	sound_deps += sfml_dep
endif
if sounds.contains('sdl-mixer')
	sound_deps += dependency('SDL2_mixer')
elif sounds.contains('sdl3-mixer')
	sound_deps += dependency('sdl3-mixer')
endif
if host_machine.system() == 'darwin' and sounds.contains('miniaudio')
	sound_deps += dependency('appleframeworks', modules: 'CoreAudio')
	sound_deps += dependency('appleframeworks', modules: 'AudioToolbox')
	if host_machine.subsystem() == 'macos'
		sound_deps += dependency('appleframeworks', modules: 'AudioUnit')
	else
		sound_deps += dependency('appleframeworks', modules: 'AVFoundation')
	endif
endif

subdir('gframe')
