#!/usr/bin/env bash

# Packages just the binaries into deploy

set -euxo pipefail

BUILD_CONFIG=${BUILD_CONFIG:-release}
TARGET_OS=${TARGET_OS:-$TRAVIS_OS_NAME}
PLATFORM=${1:-$TARGET_OS}
ARCH=${ARCH:-""}

function copy_if_exists {
    if [[ -f bin/$ARCH/$BUILD_CONFIG/$1 ]]; then
        cp bin/$ARCH/$BUILD_CONFIG/$1 deploy
    fi
}

function compress_if_exist {
    if [[ -f bin/$ARCH/$BUILD_CONFIG/$1 ]]; then
		./upx deploy/$1 -o deploy/compressed-$1
    fi
}

function strip_if_exists {
    if [[ "$BUILD_CONFIG" == "release" ]] && [[ -f bin/$ARCH/$BUILD_CONFIG/$1 ]]; then
        strip bin/$ARCH/$BUILD_CONFIG/$1
    fi
}

function bundle_if_exists {
    if [[ -f bin/$ARCH/$BUILD_CONFIG/$1.app ]]; then
        mkdir -p deploy/$1.app/Contents/MacOS
        # Binary seems to be incorrectly named with the current premake
        cp bin/$ARCH/$BUILD_CONFIG/$1.app deploy/$1.app/Contents/MacOS/$1
        # dylibbundler -x deploy/$1.app/Contents/MacOS/$1 -b -d deploy/$1.app/Contents/Frameworks/ -p @executable_path/../Frameworks/ -cd

        mkdir -p deploy/$1.app/Contents/Resources
        cp gframe/ygopro.icns deploy/$1.app/Contents/Resources/edopro.icns
        cp gframe/Info.plist deploy/$1.app/Contents/Info.plist

        if [[ -f bin/$ARCH/$BUILD_CONFIG/discord-launcher ]]; then
            mkdir -p deploy/$1.app/Contents/MacOS/discord-launcher.app/Contents/MacOS
            # Binary is named correctly and does not require external dylibs
            cp bin/$ARCH/$BUILD_CONFIG/discord-launcher deploy/$1.app/Contents/MacOS/discord-launcher.app/Contents/MacOS
            defaults write "$PWD/deploy/$1.app/Contents/MacOS/discord-launcher.app/Contents/Info.plist" "CFBundleIdentifier" "io.github.edo9300.$1.discord"
        fi
    fi
}

function bundle_if_exists_ios {
    if [[ -f bin/$ARCH/$BUILD_CONFIG/$1.app ]]; then
        mkdir -p deploy/$1.app
        # Binary seems to be incorrectly named with the current premake
        cp bin/$ARCH/$BUILD_CONFIG/$1.app deploy/$1.app/$1

        cp -r ios-assets/* deploy/$1.app/
        cp gframe/ios-Info.plist deploy/$1.app/Info.plist

        mkdir -p deploy/Payload
        cp -r deploy/$1.app deploy/Payload/EDOPro.app
        cd deploy
        zip -0 -y -r EDOPro.ipa Payload
        rm -rf deploy/Payload
        cd ..
    fi
}

mkdir -p deploy

if [[ "$PLATFORM" == "windows" ]]; then
	ARCH="."
	copy_if_exists ocgcore.dll
	copy_if_exists ygopro.exe
	compress_if_exist ygopro.exe
	copy_if_exists ygoprodll.exe
	compress_if_exist ygoprodll.exe
	copy_if_exists ygoprodll.pdb
fi
if [[ "$PLATFORM" == "linux" ]]; then
	copy_if_exists libocgcore.so
	# strip_if_exists ygopro
	copy_if_exists ygopro
	compress_if_exist ygopro
	# strip_if_exists ygoprodll
	copy_if_exists ygoprodll
	compress_if_exist ygoprodll
fi
if [[ "$PLATFORM" == "macosx" ]]; then
	copy_if_exists libocgcore.dylib
    # strip_if_exists discord-launcher
	# strip_if_exists ygopro.app
	bundle_if_exists ygopro
	# strip_if_exists ygoprodll.app
	bundle_if_exists ygoprodll
fi
if [[ "$PLATFORM" == "ios" ]]; then
	copy_if_exists libocgcore.dylib
    # strip_if_exists discord-launcher
	# strip_if_exists ygopro.app
	bundle_if_exists_ios ygopro
	# strip_if_exists ygoprodll.app
	bundle_if_exists_ios ygoprodll
fi
