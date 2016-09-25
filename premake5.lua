solution "ygo"
    location "build"
    language "C++"
    objdir "obj"

    configurations { "Release", "Debug" }

    configuration "Release"
        flags { "OptimizeSpeed" }
        targetdir "bin/release"

    configuration "Debug"
        flags "Symbols"
        defines "_DEBUG"
        targetdir "bin/debug"

    configuration { "Release", "vs*" }
        flags { "StaticRuntime", "LinkTimeOptimization" }
        disablewarnings { "4244", "4267", "4838", "4577", "4819", "4018", "4996", "4477" }

    configuration { "Debug", "vs*" }
        defines { "_ITERATOR_DEBUG_LEVEL=0" }

    configuration "vs*"
        flags "EnableSSE2"
        defines { "WIN32", "_WIN32", "WINVER=0x0501", "_CRT_SECURE_NO_WARNINGS" }

    startproject "ygopro"

    include "ocgcore"
    include "gframe"
    include "event"
    include "freetype"
    include "irrlicht"
    include "lua"
    include "sqlite3"
