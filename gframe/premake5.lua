include "lzma/."
project "ygopro"
	filter "*DLL"
		targetname "ygoprodll"
		defines "YGOPRO_BUILD_DLL"
	filter {}
	kind "ConsoleApp"
	files { "**.cpp", "**.cc", "**.c", "**.h" }
	excludes "lzma/**"
	includedirs { "../ocgcore" }

	links { "ocgcore", "clzma", "sqlite3" , "event" }
	filter "system:windows"
		includedirs { "../event/include", "../sqlite3" }
		links { "opengl32", "ws2_32", "winmm", "gdi32", "kernel32", "user32", "imm32" }

	filter "action:not vs*"
		buildoptions { "-std=c++14", "-fno-rtti", "-fpermissive" }

	filter "system:not windows"
		defines "LUA_COMPAT_5_2"
		excludes "COSOperator.*"
		links { "event_pthreads", "GL", "dl", "pthread", "lua5.3-c++" }
		linkoptions { "-Wl,-rpath '-Wl,$$ORIGIN'" }

	filter "system:bsd"
		defines "LUA_USE_POSIX"

	filter "system:macosx"
		defines "LUA_USE_MACOSX"

	filter "system:linux"
		defines "LUA_USE_LINUX"