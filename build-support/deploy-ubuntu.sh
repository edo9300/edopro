#!/usr/bin/env bash

mkdir -p deploy
mkdir -p deploy/fonts
cp bin/${BUILD_CONFIG:-release}/ygopro deploy/ygopro
cp bin/${BUILD_CONFIG:-release}/libIrrKlang.so deploy/libIrrKlang.so
cp strings.conf deploy/strings.conf
cp system.conf deploy/system.conf
cp -r textures deploy/textures
curl --retry 5 --connect-timeout 30 --location --remote-header-name --output deploy/cards.cdb https://github.com/YgoproStaff/live2019/raw/master/cards.cdb
if [[ "$BUILD_CONFIG" -eq "release" ]]; then
    strip deploy/ygopro
fi