local _includedirs=includedirs
if _ACTION=="xcode4" then
	_includedirs=sysincludedirs
end
local ygopro_config=function(static_core)
	kind "WindowedApp"
	cppdialect "C++14"
	rtti "Off"
	files { "**.cpp", "**.cc", "**.c", "**.h", "**.hpp" }
	excludes { "lzma/**", "sound_sdlmixer.*", "sound_irrklang.*", "irrklang_dynamic_loader.*", "sound_sfml.*", "sfAudio/**", "Android/**" }
	if _OPTIONS["oldwindows"] then
		filter {'action:vs*'}
			files { "../overwrites/overwrites.cpp", "../overwrites/loader.asm" }
		filter { "files:**.asm", "action:vs*" }
			exceptionhandling 'SEH'
		filter {'action:not vs*'}
			files { "../overwrites-mingw/overwrites.cpp", "../overwrites-mingw/loader.asm" }
		filter {'files:**.asm', 'action:not vs*'}
			buildmessage '%{file.relpath}'
			buildoutputs { '%{cfg.objdir}/%{file.basename}_asm.o' }
			buildcommands {
				'nasm -f win32 -o "%{cfg.objdir}/%{file.basename}_asm.o" "%{file.relpath}"'
			}
		filter {}
	end
	
	filter {'files:**.rc', 'action:not vs*'}
		buildmessage '%{file.relpath}'
		buildoutputs { '%{cfg.objdir}/%{file.basename}_rc.o' }
		buildcommands {
			'windres -DMINGW "%{file.relpath}" -o "%{cfg.objdir}/%{file.basename}_rc.o"'
		}
	filter {}

	defines "CURL_STATICLIB"
	if _OPTIONS["pics"] then
		defines { "DEFAULT_PIC_URL=" .. _OPTIONS["pics"] }
	end
	if _OPTIONS["fields"] then
		defines { "DEFAULT_FIELD_URL=" .. _OPTIONS["fields"] }
	end
	if _OPTIONS["covers"] then
		defines { "DEFAULT_COVER_URL=" .. _OPTIONS["covers"] }
	end
	if _OPTIONS["discord"] then
		defines { "DISCORD_APP_ID=" .. _OPTIONS["discord"] }
	end
	if _OPTIONS["update-url"] then
		defines { "UPDATE_URL=" .. _OPTIONS["update-url"] }
	end
	includedirs "../ocgcore"
	links { "clzma", "Irrlicht" }
	filter "system:macosx or ios"
		links { "iconv" }
	filter {}
	if _OPTIONS["no-joystick"]=="false" then
		defines "YGOPRO_USE_JOYSTICK"
		filter { "system:not windows", "configurations:Debug" }
			links { "SDL2d" }
		filter { "system:not windows", "configurations:Release" }
			links { "SDL2" }
		filter "system:macosx"
			links { "CoreAudio.framework", "AudioToolbox.framework", "CoreVideo.framework", "ForceFeedback.framework", "Carbon.framework" }
		filter {}
	end
	if _OPTIONS["sound"] then
		if _OPTIONS["sound"]=="irrklang" then
			defines "YGOPRO_USE_IRRKLANG"
			_includedirs "../irrKlang/include"
			files "sound_irrklang.*"
			files "irrklang_dynamic_loader.*"
		end
		if _OPTIONS["sound"]=="sdl-mixer" then
			defines "YGOPRO_USE_SDL_MIXER"
			files "sound_sdlmixer.*"
			filter "system:windows"
				links { "version", "setupapi" }
			filter { "system:not windows", "configurations:Debug" }
				links { "SDL2d" }
			filter { "system:not windows", "configurations:Release" }
				links { "SDL2" }
			filter "system:not windows"
				links { "SDL2_mixer", "FLAC", "mpg123", "vorbisfile", "vorbis", "ogg" }
			filter "system:macosx"
				links { "CoreAudio.framework", "AudioToolbox.framework", "CoreVideo.framework", "ForceFeedback.framework", "Carbon.framework" }
		end
		if _OPTIONS["sound"]=="sfml" then
			defines "YGOPRO_USE_SFML"
			files "sound_sfml.*"
			_includedirs "../sfAudio/include"
			links { "sfAudio" }
			filter "system:not windows"
				links { "FLAC", "vorbisfile", "vorbis", "ogg", "openal" }
				if _OPTIONS["use-mpg123"] then
					links { "mpg123" }
				end
			filter "system:macosx or ios"
				links { "CoreAudio.framework", "AudioToolbox.framework" }
			filter { "system:windows", "action:not vs*" }
				links { "FLAC", "vorbisfile", "vorbis", "ogg", "OpenAL32" }
				if _OPTIONS["use-mpg123"] then
					links { "mpg123" }
				end
		end
	end

	filter "system:windows"
		kind "ConsoleApp"
		files "ygopro.rc"
		_includedirs { "../irrlicht/include" }
		dofile("../irrlicht/defines.lua")

	filter { "system:windows", "action:vs*" }
		files "ygopro.exe.manifest"

	filter { "system:windows", "options:no-direct3d" }
		defines "NO_IRR_COMPILE_WITH_DIRECT3D_9_"

	filter { "system:windows", "options:not no-direct3d" }
		defines "IRR_COMPILE_WITH_DX9_DEV_PACK"

	filter "system:not windows"
		defines "LUA_COMPAT_5_2"
		if _OPTIONS["discord"] then
			links "discord-rpc"
		end
		links { "sqlite3", "event", "event_pthreads", "dl", "git2" }

	filter { "system:windows", "action:not vs*" }
		if _OPTIONS["discord"] then
			links "discord-rpc"
		end
		links { "sqlite3", "event", "git2" }

	filter "system:macosx or ios"
		defines "LUA_USE_MACOSX"
		links { "ssl", "crypto" }
		if os.istarget("macosx") then
			files { "*.m", "*.mm" }
			links { "ldap", "Cocoa.framework", "IOKit.framework", "OpenGL.framework", "Security.framework" }
		else
			files { "iOS/**" }
			links { "UIKit.framework", "CoreMotion.framework", "OpenGLES.framework", "Foundation.framework", "QuartzCore.framework" }
		end
		if static_core then
			links "lua"
		end

	filter { "system:macosx or ios", "configurations:Debug" }
		links { "fmtd", "curl-d", "freetyped" }

	filter { "system:macosx or ios", "configurations:Release" }
		links { "fmt", "curl", "freetype" }

	filter { "system:linux or windows", "action:not vs*", "configurations:Debug" }
		if _OPTIONS["vcpkg-root"] then
			links { "png16d", "bz2d", "fmtd", "curl-d", "freetyped" }
		else
			links { "fmt", "curl" }
		end

	filter { "system:ios" }
		files { "ios-Info.plist" }
		xcodebuildsettings {
			["PRODUCT_BUNDLE_IDENTIFIER"] = "io.github.edo9300.ygopro" .. (static_core and "" or "dll")
		}

	filter { "system:linux or windows", "action:not vs*", "configurations:Release" }
		if _OPTIONS["vcpkg-root"] then
			links { "png", "bz2", "freetype" }
		end
		links { "fmt", "curl" }

	filter "system:linux"
		defines "LUA_USE_LINUX"
		if static_core then
			links  "lua:static"
		end
		if _OPTIONS["vcpkg-root"] then
			links { "ssl", "crypto", "z", "jpeg" }
		end

	if not os.istarget("windows") then
		if _OPTIONS["vcpkg-root"] then
			for _,arch in ipairs(archs) do
				local full_vcpkg_root_path=get_vcpkg_root_path(arch)
				local platform="platforms:" .. ((arch == "armv7" and "arm") or arch)
				filter { "system:not windows", platform }
					_includedirs { full_vcpkg_root_path .. "/include/irrlicht" }
			end
		else
			filter { "system:not windows" }
				_includedirs "/usr/include/irrlicht"
		end
	end
		
		
	filter { "system:windows", "action:not vs*" }
		if static_core then
			links "lua-c++"
		end
		if _OPTIONS["vcpkg-root"] then
			links { "ssl", "crypto", "z", "jpeg" }
		end

	filter "system:not windows"
		links { "pthread" }
	
	filter "system:windows"
		links { "opengl32", "ws2_32", "winmm", "gdi32", "kernel32", "user32", "imm32", "wldap32", "crypt32", "advapi32", "rpcrt4", "ole32", "uuid", "winhttp" }
		if not _OPTIONS["oldwindows"] then
			links "Iphlpapi"
		end
end

include "lzma/."
if _OPTIONS["sound"]=="sfml" then
	include "../sfAudio"
end

if _OPTIONS["no-core"]~="true" then
	project "ygopro"
		targetname "ygopro"
		if _OPTIONS["prebuilt-core"] then
			libdirs { _OPTIONS["prebuilt-core"] }
		end
		links { "ocgcore" }
		ygopro_config(true)
end

project "ygoprodll"
	targetname "ygoprodll"
	defines "YGOPRO_BUILD_DLL"
	ygopro_config()
