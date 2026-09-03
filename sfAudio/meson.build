sfml_inc = [include_directories('./include')]

sfml_sources = [
	'src/System/FileInputStream.cpp',
	'src/System/MemoryInputStream.cpp',
	'src/System/Time.cpp',
	'src/ALCheck.cpp',
	'src/AlResource.cpp',
	'src/AudioDevice.cpp',
	'src/InputSoundFile.cpp',
	'src/Music.cpp',
	'src/SoundBuffer.cpp',
	'src/Sound.cpp',
	'src/SoundFileFactory.cpp',
	'src/SoundFileReaderFlac.cpp',
	'src/SoundFileReaderOgg.cpp',
	'src/SoundFileReaderWav.cpp',
	'src/SoundSource.cpp',
	'src/SoundStream.cpp',
]

flag_dep = dependency('flac')
vorbisfile_dep = dependency('vorbisfile')
if host_machine.system() == 'darwin'
	openal_dep = dependency('appleframeworks', modules: 'OpenAL')
else
	openal_dep = dependency('openal')
endif

sfml_defines = []

if get_option('use-mpg123')
	sfml_defines += '-DSFAUDIO_USE_MPG123'
	sfml_sources += 'src/SoundFileReaderMp3_mpg123.cpp'
	mpg123_dep = dependency('mpg123')
else
	sfml_sources += 'src/SoundFileReaderMp3_minimp3.cpp'
	mpg123_dep = dependency('', required: false)
endif

sfml_lib = static_library(
	'sfml',
	cpp_args : sfml_defines,
	include_directories: sfml_inc + include_directories('external/headers'),
	sources: sfml_sources,
    dependencies: [
        flag_dep,
		vorbisfile_dep,
		openal_dep,
		mpg123_dep,
    ],
	override_options: ['warning_level=0'],
)

sfml_dep = declare_dependency(link_with : sfml_lib, include_directories : sfml_inc)
