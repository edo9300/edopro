lzma_inc = include_directories('./')
lzma_lib = static_library(
	'lzma',
	include_directories: lzma_inc,
	sources: [
		'Alloc.c',
		'LzFind.c',
		'LzmaDec.c',
		'LzmaEnc.c',
		'LzmaLib.c',
	],
	override_options: ['warning_level=0'],
)

lzma_dep = declare_dependency(link_with : lzma_lib, include_directories : lzma_inc)
