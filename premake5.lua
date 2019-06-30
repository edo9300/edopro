newoption {
	trigger		= "no-direct3d",
	description	= "Disable directx options in irrlicht if the DirectX SDK isn't installed"
}
workspace "ygo"
	location "build"
	language "C++"
	objdir "obj"
	startproject "ygopro"

	configurations { "Debug", "DebugDLL" , "Release", "ReleaseDLL" }

	filter "system:windows"
		defines { "WIN32", "_WIN32", "NOMINMAX" }

	filter "system:bsd"
		includedirs "/usr/local/include"
		libdirs "/usr/local/lib"

	filter "system:macosx"
		toolset "clang"
		buildoptions { "-fms-extensions", "-mmacosx-version-min=10.9" }
		includedirs { "/usr/local/include", "/usr/local/include/freetype2", "/usr/local/include/irrlicht" }
		libdirs { "/usr/local/lib", "/usr/local/opt/irrlicht/lib" }
		linkoptions { "-mmacosx-version-min=10.9" }
		links { "Cocoa.framework", "IOKit.framework", "OpenGL.framework" }

	filter "action:vs*"
		vectorextensions "SSE2"
		buildoptions "-wd4996"
		defines "_CRT_SECURE_NO_WARNINGS"

	filter "action:not vs*"
		buildoptions { "-fno-strict-aliasing", "-Wno-multichar" }

	filter { "action:not vs*", "system:windows" }
	  buildoptions { "-static-libgcc" }

	filter "configurations:Debug*"
		symbols "On"
		defines "_DEBUG"
		targetdir "bin/debug"

	filter { "configurations:Release*" , "action:not vs*" }
		symbols "On"
		defines "NDEBUG"
		buildoptions "-march=native"

	filter "configurations:Release*"
		optimize "Speed"
		targetdir "bin/release"

	include "ocgcore"
	include "gframe"
	if os.istarget("windows") then
 		include "irrlicht"
	end
