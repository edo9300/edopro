#!/usr/bin/env bash

mkdir -p deploy
mkdir -p deploy/fonts
mkdir -p deploy/ygopro.app/Contents/MacOS
# Binary seems to be incorrectly named with the current premake
cp bin/${BUILD_CONFIG:-release}/ygopro.app deploy/ygopro.app/Contents/MacOS/ygopro
dylibbundler -x deploy/ygopro.app/Contents/MacOS/ygopro -b -d deploy/ygopro.app/Contents/Frameworks/ -p @executable_path/../Frameworks/ -cd
# OpenSSL isn't in /usr/local/lib because Apple has deprecated it. 
# libssl for some reason doesn't link to the libcrypto symlink in /usr/local/opt/openssl/lib,
# but directly to the Cellar location, and this isn't caught by dylibbundler
# This line likely needs to be updated if libcrypto's version ever changes, but not openssl's version
install_name_tool -change /usr/local/Cellar/openssl/$(brew list --versions openssl | cut -d " " -f 2)/lib/libcrypto.1.0.0.dylib @executable_path/../Frameworks/libcrypto.1.0.0.dylib deploy/ygopro.app/Contents/Frameworks/libssl.1.0.0.dylib
cp strings.conf deploy/strings.conf
cp system.conf deploy/system.conf
cp -r textures deploy/textures
curl --retry 5 --connect-timeout 30 --location --remote-header-name --output deploy/cards.cdb https://github.com/YgoproStaff/live2019/raw/master/cards.cdb
if [[ "$BUILD_CONFIG" -eq "release" ]]; then
    strip deploy/ygopro.app/Contents/MacOS/ygopro
fi