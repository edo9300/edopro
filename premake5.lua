newoption {
	trigger	= "no-direct3d",
	description = "Disable DirectX options in irrlicht if the DirectX SDK isn't installed"
}
newoption {
	trigger = "oldwindows",
	description = "Use some tricks to support up to windows 2000"
}
newoption {
	trigger = "sound",
	value = "backend",
	description = "Choose sound backend",
	allowed = {
		{ "irrklang",  "irrklang" },
		{ "sdl-mixer",  "SDL2-mixer" },
		{ "sfml",  "SFML" }
	}
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
	description = "Ignore the ocgcore subproject and only generate the solution for yroprodll"
}
newoption {
	trigger = "architecture",
	value = "arch",
	description = "Architecture for the solution, allowed values are x86, x64, arm64, armv7, comma separated"
}

local function default_arch()
	if os.istarget("linux") or os.istarget("macosx") then return "x64" end
	if os.istarget("windows") then return "x86" end
	if os.istarget("ios") then return "arm64" end
end

local function valid_arch(arch)
	return arch == "x86" or arch == "x64" or arch == "arm64" or arch == "armv7"
end

local absolute_vcpkg_path =(function()
	if _OPTIONS["vcpkg-root"] then
		return path.getabsolute(_OPTIONS["vcpkg-root"])
	end
end)()

function get_vcpkg_root_path(arch)
	local function vcpkg_triplet_path()
		if os.istarget("linux") then
			return "-linux"
		elseif os.istarget("macosx") then
			return "-osx"
		elseif os.istarget("windows") then
			return "-mingw-static"
		elseif os.istarget("ios") then
			return "-ios"
		end
	end
	return absolute_vcpkg_path .. "/installed/" .. arch .. vcpkg_triplet_path()
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

local _includedirs=includedirs
if _ACTION=="xcode4" then
	_includedirs=sysincludedirs
end
workspace "ygo"
	location "build"
	language "C++"
	objdir "obj"
	startproject "ygopro"
	staticruntime "on"

	configurations { "Debug", "Release" }

	filter "system:windows"
		systemversion "latest"
		defines { "WIN32", "_WIN32", "NOMINMAX" }
		for arch in ipairs(archs) do
			if arch=="x86" then platforms "Win32" end
			if arch=="x64" then platforms "x64" end
		end

	filter "system:not windows"
		platforms(archs)

	filter "platforms:Win32"
		architecture "x86"

	filter "platforms:x86"
		architecture "x86"

	filter "platforms:x64"
		architecture "x64"

	filter "platforms:arm64"
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
		filter { "action:vs2015" }
			toolset "v140_xp"
		filter { "action:vs*", "action:not vs2015" }
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
			local platform="platforms:" .. (arch=="x86" and os.istarget("windows") and "Win32" or (arch == "armv7" and "arm") or arch)
			filter { "action:not vs*", platform }
				_includedirs { full_vcpkg_root_path .. "/include" }

			filter { "action:not vs*", "configurations:Debug", platform }
				libdirs { full_vcpkg_root_path .. "/debug/lib" }

			filter { "action:not vs*", "configurations:Release", platform }
				libdirs { full_vcpkg_root_path .. "/lib" }
		end
	end

	filter "system:macosx"
		defines { "GL_SILENCE_DEPRECATION" }
		_includedirs { "/usr/local/include" }
		libdirs { "/usr/local/lib" }
		--systemversion "10.10"

	filter "system:ios"
		systemversion "9.0"
		buildoptions { "-mios-simulator-version-min=9.0" }
		linkoptions { "-mios-simulator-version-min=9.0" }

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
		
	filter { "configurations:Debug", "architecture:ARM64" }
		targetdir "bin/arm64/debug"
		
	filter { "configurations:Release", "architecture:ARM" }
		targetdir "bin/armv7/debug"

	filter { "configurations:Release*" , "action:not vs*" }
		symbols "On"
		defines "NDEBUG"

	filter "configurations:Release"
		optimize "Size"
		flags "LinkTimeOptimization"
		targetdir "bin/release"
		
	filter { "configurations:Release", "architecture:x64" }
		targetdir "bin/x64/release"
		
	filter { "configurations:Release", "architecture:ARM64" }
		targetdir "bin/arm64/release"
		
	filter { "configurations:Release", "architecture:ARM" }
		targetdir "bin/armv7/release"
	
	filter { "system:linux", "configurations:Release" }
		linkoptions { "-static-libgcc", "-static-libstdc++" }

	subproject = true
	if not _OPTIONS["prebuilt-core"] and not _OPTIONS["no-core"] then
		include "ocgcore"
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
