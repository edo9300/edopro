include "lzma/."
project "ygopro"
	configuration "*DLL"
		targetname "ygoprodll"
		defines "YGOPRO_BUILD_DLL"
	configuration {}
	kind "ConsoleApp"
	files { "**.cpp", "**.cc", "**.c", "**.h" }
	excludes "lzma/**"
	includedirs { "../ocgcore" }

	links { "ocgcore", "clzma", "sqlite3" , "event" }
	configuration "windows"
		includedirs { "../event/include", "../sqlite3" }
		links { "opengl32", "ws2_32", "winmm", "gdi32", "kernel32", "user32", "imm32" }

	configuration "not vs*"
		buildoptions { "-std=c++14", "-fno-rtti", "-fpermissive" }

	configuration "not windows"
		defines "LUA_COMPAT_5_2"
		excludes "COSOperator.*"
		links { "event_pthreads", "GL", "dl", "pthread", "lua5.3-c++" }
		linkoptions { "-Wl,-rpath '-Wl,$$ORIGIN'" }

	configuration "macosx"
		defines "LUA_USE_MACOSX"

	configuration "bsd"
		defines "LUA_USE_POSIX"

	configuration "linux"
		defines "LUA_USE_LINUX"
