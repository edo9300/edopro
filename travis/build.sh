#!/usr/bin/env bash

set -euo pipefail

PICS_URL=${PICS_URL:-""}
FIELDS_URL=${FIELDS_URL:-""}
COVERS_URL=${COVERS_URL:-""}
DISCORD_APP_ID=${DISCORD_APP_ID:-""}
UPDATE_URL=${UPDATE_URL:-""}
ARCH=${ARCH:-"x64"}
TARGET_OS=${TARGET_OS:-""}
VCPKG_TRIPLET=${VCPKG_TRIPLET:-""}
BUNDLED_FONT=""
if [[ -f "NotoSansJP-Regular.otf" ]]; then
	BUNDLED_FONT="--bundled-font=NotoSansJP-Regular.otf"
fi

if [[ "$TRAVIS_OS_NAME" == "windows" ]]; then
	if [[ -z "${VS_GEN:-""}" ]]; then
		./premake5 vs2017 $BUNDLED_FONT --no-core=true --oldwindows=true --sound=miniaudio,sfml --no-joystick=true --pics=\"$PICS_URL\" --fields=\"$FIELDS_URL\" --covers=\"$COVERS_URL\" --discord=\"$DISCORD_APP_ID\" --update-url=\"$UPDATE_URL\"
		msbuild.exe -m -p:Configuration=$BUILD_CONFIG -p:Platform=Win32 ./build/ygo.sln -t:ygoprodll -verbosity:minimal -p:EchoOff=true
	else
		./premake5 $VS_GEN $BUNDLED_FONT --no-core=true --sound=miniaudio,sfml --no-joystick=true --pics=\"$PICS_URL\" --fields=\"$FIELDS_URL\" --covers=\"$COVERS_URL\" --discord=\"$DISCORD_APP_ID\" --update-url=\"$UPDATE_URL\"
		msbuild.exe -m -p:Configuration=$BUILD_CONFIG -p:Platform=Win32 ./build/ygo.sln -t:ygoprodll -verbosity:minimal -p:EchoOff=true
	fi
	exit 0
fi
PREMAKE_FLAGS=""
SOUND_BACKEND="sfml"
if [[ -n "${ARCH:-""}" ]]; then
	PREMAKE_FLAGS=" --architecture=$ARCH"
fi
if [[ -n "${TARGET_OS:-""}" ]]; then
	PREMAKE_FLAGS="$PREMAKE_FLAGS --os=$TARGET_OS"
fi
if [[ -n "${VCPKG_TRIPLET:-""}" ]]; then
	PREMAKE_FLAGS="$PREMAKE_FLAGS --vcpkg-triplet=$VCPKG_TRIPLET"
fi
if [[ "$TARGET_OS" == "windows" ]]; then
	SOUND_BACKEND="miniaudio,sfml"
fi
./premake5 gmake2 $PREMAKE_FLAGS $BUNDLED_FONT --no-core=true --no-direct3d --vcpkg-root=$VCPKG_ROOT --sound=$SOUND_BACKEND --no-joystick=true --pics=\"$PICS_URL\" --fields=\"$FIELDS_URL\" --covers=\"$COVERS_URL\" --discord=\"$DISCORD_APP_ID\" --update-url=\"$UPDATE_URL\"
PROCS=""
if [[ "$TRAVIS_OS_NAME" == "macosx" ]]; then
    PROCS=$(sysctl -n hw.ncpu)
else
    PROCS=$(nproc)
fi
WINDRES_ARG=""
if [[ "$TARGET_OS" == "windows" ]]; then
	WINDRES_ARG="RESCOMP=i686-w64-mingw32-windres"
fi
if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
    make $WINDRES_ARG -Cbuild -j$PROCS config="${BUILD_CONFIG}_${ARCH}" ygoprodll
fi
if [[ "$TRAVIS_OS_NAME" == "macosx" ]]; then
    AR=ar make -Cbuild -j$PROCS config="${BUILD_CONFIG}_${ARCH}" ygoprodll
fi
