#!/usr/bin/env bash

mkdir -p deploy

if [[ $BUILD_CONFIG == *"dll" ]]; then
    EXECUTABLE=ygoprodll
    # Strip off "dll" since the built files are in the same spot as the non-DLL variant
    BUILD_CONFIG=${BUILD_CONFIG%???}
    cp bin/${BUILD_CONFIG}/*.dylib deploy/
else
    EXECUTABLE=ygopro
fi

mkdir -p deploy/fonts
mkdir -p deploy/$EXECUTABLE.app/Contents/MacOS
# Binary seems to be incorrectly named with the current premake
cp bin/${BUILD_CONFIG:-release}/$EXECUTABLE.app deploy/$EXECUTABLE.app/Contents/MacOS/$EXECUTABLE
dylibbundler -x deploy/$EXECUTABLE.app/Contents/MacOS/$EXECUTABLE -b -d deploy/$EXECUTABLE.app/Contents/Frameworks/ -p @executable_path/../Frameworks/ -cd
# OpenSSL isn't in /usr/local/lib because Apple has deprecated it.
# libssl for some reason doesn't link to the libcrypto symlink in /usr/local/opt/openssl/lib,
# but directly to the Cellar location, and this isn't caught by dylibbundler
# This line likely needs to be updated if libcrypto's version ever changes, but not openssl's version
install_name_tool -change /usr/local/Cellar/openssl/1.0.2s/lib/libcrypto.1.0.0.dylib @executable_path/../Frameworks/libcrypto.1.0.0.dylib deploy/$EXECUTABLE.app/Contents/Frameworks/libssl.1.0.0.dylib
cp strings.conf deploy/strings.conf
cp system.conf deploy/system.conf
cp -r textures deploy/textures
curl --retry 5 --connect-timeout 30 --location --remote-header-name --output deploy/cards.cdb https://github.com/YgoproStaff/live2019/raw/master/cards.cdb
if [[ "$BUILD_CONFIG" -eq "release" ]]; then
    strip deploy/$EXECUTABLE.app/Contents/MacOS/$EXECUTABLE
fi