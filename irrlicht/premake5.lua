project "Irrlicht"
	kind "StaticLib"

	includedirs { "include", "$(DXSDK_DIR)Include" } 
	libdirs "$(DXSDK_DIR)Lib/x86"
	dofile("../irrlicht defines.lua")
	defines { "NO_IRR_USE_NON_SYSTEM_ZLIB_", "NO_IRR_USE_NON_SYSTEM_BZLIB_", "NO_IRR_USE_NON_SYSTEM_LIB_PNG_", "NO_IRR_USE_NON_SYSTEM_JPEG_LIB_" }
	exceptionhandling "Off"
	rtti "Off"
	files { "**.cpp", "**.c", "**.cxx", "**.hpp", "**.h" }

	filter "options:no-direct3d"
		defines "NO_IRR_COMPILE_WITH_DIRECT3D_9_"

	filter "options:not no-direct3d"
		defines "IRR_COMPILE_WITH_DX9_DEV_PACK"

	filter "action:vs*"
		defines { "IRRLICHT_FAST_MATH", "UNICODE", "_UNICODE" }

	filter "system:windows"
		links "imm32"