local _includedirs=includedirs
if _ACTION=="xcode4" then
	_includedirs=sysincludedirs
end
local ygopro_config=function(static_core)
	kind "WindowedApp"
	cppdialect "C++17"
	rtti "Off"
	files { "**.cpp", "**.cc", "**.c", "**.h", "**.hpp" }
	excludes { "lzma/**", "SoundBackends/**", "sfAudio/**", "Android/**" }
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

	filter { "action:not vs*" }
		enablewarnings "pedantic"
	filter {}

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
	if _OPTIONS["discord"] and not os.istarget("ios") then
		defines { "DISCORD_APP_ID=" .. _OPTIONS["discord"] }
	end
	if _OPTIONS["update-url"] then
		defines { "UPDATE_URL=" .. _OPTIONS["update-url"] }
	end
	if _OPTIONS["bundled-font"] then
		defines "YGOPRO_USE_BUNDLED_FONT"
	else
		excludes { "CGUITTFont/bundled_font.cpp" }
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
	if next(sounds) ~= nil then
		if sounds.irrklang then
			defines "YGOPRO_USE_IRRKLANG"
			_includedirs "../irrKlang/include"
			files "SoundBackends/irrklang/**"
			filter {}
		end
		if sounds["sdl-mixer"] then
			defines "YGOPRO_USE_SDL_MIXER"
			files "SoundBackends/sdlmixer/**"
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
			filter {}
		end
		if sounds["sdl3-mixer"] then
			defines "YGOPRO_USE_SDL_MIXER3"
			files "SoundBackends/sdlmixer3/**"
			filter "system:windows"
				links { "version", "setupapi" }
			filter { "system:not windows", "configurations:Debug" }
				links { "SDL2d" }
			filter { "system:not windows", "configurations:Release" }
				links { "SDL3" }
			filter "system:not windows"
				links { "SDL3_mixer", "FLAC", "mpg123", "vorbisfile", "vorbis", "ogg" }
			filter "system:macosx"
				links { "CoreAudio.framework", "AudioToolbox.framework", "CoreVideo.framework", "ForceFeedback.framework", "Carbon.framework" }
			filter {}
		end
		if sounds.sfml then
			defines "YGOPRO_USE_SFML"
			files "SoundBackends/sfml/**"
			_includedirs "../sfAudio/include"
			links { "sfAudio" }
			filter "system:not windows"
				links { "FLAC", "vorbisfile", "vorbis", "ogg", "openal" }
				if _OPTIONS["use-mpg123"] then
					links { "mpg123" }
				end
			filter "system:macosx or ios"
				links { "CoreAudio.framework", "AudioToolbox.framework" }
			filter "system:macosx"
				links { "AudioUnit.framework" }
			filter { "system:windows", "action:not vs*" }
				links { "FLAC", "vorbisfile", "vorbis", "ogg", "OpenAL32" }
				if _OPTIONS["use-mpg123"] then
					links { "mpg123" }
				end
			filter {}
		end
		if sounds.miniaudio then
			defines "YGOPRO_USE_MINIAUDIO"
			files "SoundBackends/miniaudio/**"
			filter { "system:ios", "files:**sound_miniaudio.cpp" }
				compileas "Objective-C++"
			filter "system:macosx or ios"
				defines "MA_NO_RUNTIME_LINKING"
				links { "CoreAudio.framework", "AudioToolbox.framework" }
			filter "system:macosx"
				links { "AudioUnit.framework" }
			filter "system:ios"
				links { "AVFoundation.framework" }
			filter {}
		end
		files "SoundBackends/sound_threaded_backend.*"
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
		if _OPTIONS["discord"] and not os.istarget("ios") then
			links "discord-rpc"
		end
		links { "sqlite3", "event", "event_pthreads", "dl", "git2", "ssh2" }

	filter { "system:windows", "action:not vs*" }
		if _OPTIONS["discord"] then
			links "discord-rpc"
		end
		links { "sqlite3", "event", "git2", "ssh2" }

	filter "system:macosx or ios"
		links { "ssl", "crypto" }
		if os.istarget("macosx") then
			files { "*.m", "*.mm" }
			links { "ldap", "Cocoa.framework", "IOKit.framework", "OpenGL.framework", "Security.framework", "SystemConfiguration.framework" }
		else
			files { "iOS/**" }
			links { "UIKit.framework", "CoreMotion.framework", "OpenGLES.framework", "Foundation.framework", "QuartzCore.framework" }
		end

	filter { "system:macosx or ios", "configurations:Debug" }
		links { "fmtd", "curl-d", "freetyped" }

	filter { "system:macosx or ios", "configurations:Release" }
		links { "fmt", "curl", "freetype" }

	filter { "system:linux or windows", "action:not vs*", "configurations:Debug" }
		if _OPTIONS["vcpkg-root"] then
			links { "png16d", "bz2d", "fmtd", "curl-d", "freetyped" }
		else
			links { "fmt", "curl", "freetype" }
		end

	filter { "system:ios" }
		files { "ios-Info.plist" }
		xcodebuildsettings {
			["PRODUCT_BUNDLE_IDENTIFIER"] = "io.github.edo9300.ygopro" .. (static_core and "" or "dll")
		}

	filter { "system:linux or windows", "action:not vs*", "configurations:Release" }
		if _OPTIONS["vcpkg-root"] then
			links { "png", "bz2" }
		end
		links { "fmt", "curl", "freetype" }

	if _OPTIONS["vcpkg-root"] then
		filter "system:linux"
			links { "ssl", "crypto", "z", "jpeg" }
	end

	if not os.istarget("windows") then
		if _OPTIONS["vcpkg-root"] then
			for _,arch in ipairs(archs) do
				local full_vcpkg_root_path=get_vcpkg_root_path(arch)
				local platform="platforms:" .. arch
				filter { "system:not windows", platform }
					_includedirs { full_vcpkg_root_path .. "/include/irrlicht" }
			end
		else
			filter { "system:not windows" }
				_includedirs "/usr/include/irrlicht"
		end
	end


	filter { "system:windows", "action:not vs*" }
		if _OPTIONS["vcpkg-root"] then
			links { "ssl", "crypto", "zlib", "jpeg" }
		end

	filter "system:not windows"
		links { "pthread" }

	filter "system:windows"
		links { "wbemuuid", "opengl32", "ws2_32", "winmm", "gdi32", "kernel32", "user32", "imm32", "wldap32", "crypt32", "advapi32", "rpcrt4", "ole32", "OleAut32", "uuid", "winhttp", "Secur32" }
		if not _OPTIONS["oldwindows"] then
			links "Iphlpapi"
		end

	if static_core then
		filter {}
			links "lua"
	end
end

include "lzma/."
if sounds.sfml then
	include "../sfAudio"
end

if not _OPTIONS["no-core"] then
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
