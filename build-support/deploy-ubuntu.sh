#!/usr/bin/env bash

mkdir -p deploy

if [[ $BUILD_CONFIG == *"dll" ]]; then
    EXECUTABLE=ygoprodll
    # Strip off "dll" since the built files are in the same spot as the non-DLL variant
    BUILD_CONFIG=${BUILD_CONFIG%???}
    cp bin/${BUILD_CONFIG}/*.so deploy/
else
    EXECUTABLE=ygopro
fi

mkdir -p deploy/fonts
cp bin/${BUILD_CONFIG:-release}/$EXECUTABLE deploy/
cp strings.conf deploy/strings.conf
cp system.conf deploy/system.conf
cp -r textures deploy/textures
curl --retry 5 --connect-timeout 30 --location --remote-header-name --output deploy/cards.cdb https://github.com/YgoproStaff/live2019/raw/master/cards.cdb
if [[ "$BUILD_CONFIG" -eq "release" ]]; then
    strip deploy/$EXECUTABLE
fi