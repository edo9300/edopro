newoption {
	trigger	= "no-direct3d",
	description = "Disable DirectX options in irrlicht if the DirectX SDK isn't installed"
}
newoption {
	trigger = "oldwindows",
	description = "Use some tricks to support up to windows XP sp3"
}
newoption {
	trigger = "sound",
	value = "backends",
	description = "Choose sound backend",
	description = "Sound backends for the solution, allowed values are any combination of irrklang, sdl-mixer, sdl3-mixer, sfml and miniaudio, comma separated"
}
newoption {
	trigger = "use-mpg123",
	description = "Use mpg123 mp3 backend instead of minimp3 (Available only when using SFML audio backend)"
}
newoption {
	trigger = "no-joystick",
	default = "true",
	description = "Add base joystick compatibility (Requires SDL2)"
}
newoption {
	trigger = "pics",
	value = "url_template",
	description = "Default URL for card images"
}
newoption {
	trigger = "fields",
	value = "url_template",
	description = "Default URL for Field Spell backgrounds"
}
newoption {
	trigger = "covers",
	value = "url_template",
	description = "Default URL for cover images"
}
newoption {
	trigger = "prebuilt-core",
	value = "path",
	description = "Path to library folder containing libocgcore"
}
newoption {
	trigger = "vcpkg-root",
	value = "path",
	description = "Path to vcpkg installation"
}
newoption {
	trigger = "discord",
	value = "app_id_token",
	description = "Discord App ID for rich presence"
}
newoption {
	trigger = "update-url",
	value = "url",
	description = "API endpoint to check for updates from"
}
newoption {
	trigger = "no-core",
	description = "Ignore the ocgcore subproject and only generate the solution for ygoprodll"
}
newoption {
	trigger = "architecture",
	value = "arch",
	description = "Architecture for the solution, allowed values are x86, x64, arm64, armv7, comma separated"
}
newoption {
	trigger = "bundled-font",
	value = "font",
	description = "Path to a font file that will be bundled in the client and used as fallback font for missing glyphs"
}

local function default_arch()
	if os.istarget("linux") or os.istarget("macosx") then return "x64" end
	if os.istarget("windows") then return "x86" end
	if os.istarget("ios") then return "arm64" end
end

local function valid_arch(arch)
	return arch == "x86" or arch == "x64" or arch == "arm64" or arch == "armv7"
		or arch == "x86-iossim" or arch == "x64-iossim" or arch == "arm64-iossim"
end

local function valid_sound(sound)
	return sound == "irrklang" or sound == "sdl-mixer" or sound == "sfml" or sound == "miniaudio"
end

local absolute_vcpkg_path =(function()
	if _OPTIONS["vcpkg-root"] then
		return path.getabsolute(_OPTIONS["vcpkg-root"])
	end
end)()

local function ends_with(str, ending)
   return str:sub(-#ending) == ending
end

function get_vcpkg_root_path(arch)
	local function vcpkg_triplet_path()
		if os.istarget("linux") then
			return "-linux"
		elseif os.istarget("macosx") then
			return "-osx"
		elseif os.istarget("windows") then
			return "-mingw-static"
		elseif os.istarget("ios") then
			if ends_with(arch, "iossim") then
				return ""
			else
				return "-ios"
			end
		end
	end
	return absolute_vcpkg_path .. "/installed/" .. ((arch == "armv7" and "arm") or arch) .. vcpkg_triplet_path()
end

archs={}

if _OPTIONS["architecture"] then
	for arch in string.gmatch(_OPTIONS["architecture"], "([^,]+)") do
		if valid_arch(arch) then
			table.insert(archs,arch)
		end
	end
end

if #archs == 0 then archs = { default_arch() } end

sounds={}

if _OPTIONS["sound"] then
	print(_OPTIONS["sound"])
	for sound in string.gmatch(_OPTIONS["sound"], "([^,]+)") do
		if valid_sound(sound) then
			print(sound)
			sounds[sound]=true
		end
	end
end

local _includedirs=includedirs
if _ACTION=="xcode4" then
	_includedirs=sysincludedirs
end
workspace "ygo"
	location "build"
	language "C++"
	objdir "obj"
	if not _OPTIONS["no-core"] then
		startproject "ygopro"
	else
		startproject "ygoprodll"
	end
	staticruntime "on"

	warnings "Extra"
	filter { "action:vs*" }
		disablewarnings "4100" --'identifier' : unreferenced formal parameter
		disablewarnings "4244" --conversion from 'T1' to 'T2'. Possible loss of data
	filter { "action:not vs*" }
		disablewarnings { "unknown-warning-option", "unused-parameter", "unknown-pragmas", "ignored-qualifiers", "missing-field-initializers", "implicit-const-int-float-conversion", "missing-braces", "invalid-utf8" }
	filter { "action:not vs*", "files:**.cpp" }
		disablewarnings { "deprecated-copy", "unused-lambda-capture" }
	filter{}

	configurations { "Debug", "Release" }

	filter "system:windows"
		systemversion "latest"
		defines { "WIN32", "_WIN32", "NOMINMAX" }
		for _,arch in ipairs(archs) do
			if arch=="x86" then platforms "Win32" end
			if arch=="x64" then platforms "x64" end
		end

	filter "system:not windows"
		platforms(archs)

	filter "platforms:Win32"
		architecture "x86"

	filter "platforms:x86*"
		architecture "x86"

	filter "platforms:x64*"
		architecture "x64"

	filter "platforms:arm64*"
		architecture "ARM64"

	filter "platforms:armv7"
		architecture "ARM"

	if os.istarget("ios") and _ACTION~="xcode4" then
		premake.tools.clang.ldflags.architecture.x86 = "-arch i386"
		premake.tools.clang.ldflags.architecture.x86_64 = "-arch x86_64"
		premake.tools.clang.ldflags.architecture.ARM = "-arch armv7"
		premake.tools.clang.ldflags.architecture.ARM64 = "-arch arm64"
		premake.tools.clang.shared.architecture = premake.tools.clang.ldflags.architecture
		premake.tools.gcc.ldflags.architecture = premake.tools.clang.ldflags.architecture
		premake.tools.gcc.shared.architecture = premake.tools.clang.ldflags.architecture
	end

	if _OPTIONS["oldwindows"] then
		filter { "action:vs*" }
			toolset "v141_xp"
		filter {}
	else
		filter { "action:vs*" }
			systemversion "latest"
	end


	if _OPTIONS["vcpkg-root"] then
		for _,arch in ipairs(archs) do
			local full_vcpkg_root_path=get_vcpkg_root_path(arch)
			print(full_vcpkg_root_path)
			local platform="platforms:" .. (arch=="x86" and os.istarget("windows") and "Win32" or arch)
			filter { "action:not vs*", platform }
				_includedirs { full_vcpkg_root_path .. "/include" }

			filter { "action:not vs*", "configurations:Debug", platform }
				libdirs { full_vcpkg_root_path .. "/debug/lib" }

			filter { "action:not vs*", "configurations:Release", platform }
				libdirs { full_vcpkg_root_path .. "/lib" }
		end
	end

	filter "system:macosx"
		_includedirs { "/usr/local/include" }
		libdirs { "/usr/local/lib" }
		--systemversion "10.10"

	filter { "system:ios", "platforms:*-iossim"}
		buildoptions { "-mios-simulator-version-min=9.0" }
		linkoptions { "-mios-simulator-version-min=9.0" }
	filter { "system:ios", "platforms:arm64 or armv7"}
		buildoptions { "-miphoneos-version-min=9.0" }
		linkoptions { "-miphoneos-version-min=9.0" }

	filter "action:vs*"
		vectorextensions "SSE2"
		buildoptions "-wd4996"
		defines "_CRT_SECURE_NO_WARNINGS"

	filter "action:not vs*"
		buildoptions { "-fno-strict-aliasing", "-Wno-multichar" }

	filter { "action:not vs*", "system:windows" }
		buildoptions { "-static-libgcc", "-static-libstdc++", "-static", "-lpthread" }
		linkoptions { "-mthreads", "-municode", "-static-libgcc", "-static-libstdc++", "-static", "-lpthread" }
		defines { "UNICODE", "_UNICODE" }

	filter { "action:not vs*", "system:windows", "configurations:Release" }
		buildoptions { "-s" }
		linkoptions { "-s" }

	filter "configurations:Debug"
		symbols "On"
		defines "_DEBUG"
		targetdir "bin/debug"
		runtime "Debug"

	filter { "configurations:Debug", "architecture:x64" }
		targetdir "bin/x64/debug"

	filter { "configurations:Debug", "platforms:x64-iossim" }
		targetdir "bin/x64-iossim/debug"

	filter { "configurations:Debug", "architecture:ARM64" }
		targetdir "bin/arm64/debug"

	filter { "configurations:Debug", "platforms:arm64-iossim" }
		targetdir "bin/arm64-iossim/debug"

	filter { "configurations:Debug", "architecture:ARM" }
		targetdir "bin/armv7/debug"

	filter { "configurations:Release*" , "action:not vs*" }
		symbols "On"

	filter "configurations:Release"
		optimize "Size"
		targetdir "bin/release"
		defines "NDEBUG"

	filter { "configurations:Release", "action:vs* or system:not windows" }
		if linktimeoptimization then
			linktimeoptimization "On"
		else
			flags "LinkTimeOptimization"
		end

	filter { "configurations:Release", "architecture:x64" }
		targetdir "bin/x64/release"

	filter { "configurations:Release", "platforms:x64-iossim" }
		targetdir "bin/x64-iossim/release"

	filter { "configurations:Release", "architecture:ARM64" }
		targetdir "bin/arm64/release"

	filter { "configurations:Release", "platforms:arm64-iossim" }
		targetdir "bin/arm64-iossim/release"

	filter { "configurations:Release", "architecture:ARM" }
		targetdir "bin/armv7/release"

	filter { "system:linux", "configurations:Release" }
		linkoptions { "-static-libgcc", "-static-libstdc++" }

	subproject = true
	if not _OPTIONS["prebuilt-core"] and not _OPTIONS["no-core"] then
		include "ocgcore"
	end
	if _OPTIONS["bundled-font"] then
		local bin2c=require("tools.bin2c")
		bin2c(_OPTIONS["bundled-font"], "gframe/CGUITTFont/bundled_font.cpp")
	end
	include "gframe"
	if os.istarget("windows") then
		include "irrlicht"
	end
	if os.istarget("macosx") and _OPTIONS["discord"] then
		include "discord-launcher"
	end

local function vcpkgStaticTriplet(prj)
	premake.w('<VcpkgTriplet Condition="\'$(Platform)\'==\'Win32\'">x86-windows-static</VcpkgTriplet>')
	premake.w('<VcpkgTriplet Condition="\'$(Platform)\'==\'x64\'">x64-windows-static</VcpkgTriplet>')
end

local function disableWinXPWarnings(prj)
	premake.w('<XPDeprecationWarning>false</XPDeprecationWarning>')
end

local function vcpkgStaticTriplet202006(prj)
	premake.w('<VcpkgEnabled>true</VcpkgEnabled>')
	premake.w('<VcpkgUseStatic>true</VcpkgUseStatic>')
	premake.w('<VcpkgAutoLink>true</VcpkgAutoLink>')
end

require('vstudio')

premake.override(premake.vstudio.vc2010.elements, "globals", function(base, prj)
	local calls = base(prj)
	table.insertafter(calls, premake.vstudio.vc2010.targetPlatformVersionGlobal, vcpkgStaticTriplet)
	table.insertafter(calls, premake.vstudio.vc2010.targetPlatformVersionGlobal, disableWinXPWarnings)
	table.insertafter(calls, premake.vstudio.vc2010.globals, vcpkgStaticTriplet202006)
	return calls
end)
