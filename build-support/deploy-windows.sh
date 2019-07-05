#!/usr/bin/env bash

mkdir -p deploy
mkdir -p deploy/fonts
if [[ $BUILD_CONFIG == *"dll" ]]; then
    BUILD_CONFIG=${BUILD_CONFIG%???}
    cp bin/${BUILD_CONFIG}/*.dll deploy/
fi
cp bin/${BUILD_CONFIG:-release}/*.exe  deploy/
if [[ "${1:-64}" -eq "64" ]]; then
    cp irrKlang/bin/winx64-visualStudio/irrKlang.dll deploy/irrKlang.dll
elif [[ "$1" -eq "32" ]]; then
    cp irrKlang/bin/win32-visualStudio/irrKlang.dll deploy/irrKlang.dll
fi
cp strings.conf deploy/strings.conf
cp system.conf deploy/system.conf
cp -r textures deploy/textures
curl --retry 5 --connect-timeout 30 --location --remote-header-name --output deploy/cards.cdb https://github.com/YgoproStaff/live2019/raw/master/cards.cdb