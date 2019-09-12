workspace "ygo"
	location "build"
	language "C++"
	objdir "obj"
	startproject "ygopro"

	configurations { "Debug", "Release" }

	filter "system:windows"
		defines { "WIN32", "_WIN32", "NOMINMAX" }

	filter "system:not windows"
		includedirs "/usr/local/include"
		libdirs "/usr/local/lib"

	filter "action:vs*"
		vectorextensions "SSE2"
		buildoptions "-wd4996"
		defines "_CRT_SECURE_NO_WARNINGS"

	filter "action:not vs*"
		buildoptions { "-fno-strict-aliasing", "-Wno-multichar" }

	filter { "action:not vs*", "system:windows" }
	  buildoptions { "-static-libgcc" }

	filter "configurations:Debug"
		symbols "On"
		defines "_DEBUG"
		targetdir "bin/debug"

	filter { "configurations:Release" , "action:not vs*" }
		symbols "On"
		defines "NDEBUG"

	filter "configurations:Release"
		optimize "Size"
		targetdir "bin/release"
	subproject = true
	include "ocgcore"
	include "gframe"
	if os.istarget("windows") then
		include "event"
		include "sqlite3"
		include "lua"
	end
