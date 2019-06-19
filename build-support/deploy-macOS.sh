#!/usr/bin/env bash

mkdir -p deploy
mkdir -p deploy/fonts
mkdir -p deploy/ygopro.app/Contents/MacOS
# Binary seems to be incorrectly named with the current premake
cp bin/${BUILD_CONFIG:-release}/ygopro.app deploy/ygopro.app/Contents/MacOS/ygopro
dylibbundler -x deploy/ygopro.app/Contents/MacOS/ygopro -b -d deploy/ygopro.app/Contents/Frameworks/ -p @executable_path/../Frameworks/ -cd
cp strings.conf deploy/strings.conf
cp system.conf deploy/system.conf
cp -r textures deploy/textures
curl --retry 5 --connect-timeout 30 --location --remote-header-name --output deploy/cards.cdb https://github.com/YgoproStaff/live2019/raw/master/cards.cdb
if [[ "$BUILD_CONFIG" -eq "release" ]]; then
    strip deploy/ygopro.app/Contents/MacOS/ygopro
fi