#!/usr/bin/env bash

set -euxo pipefail

./travis/install-local-dependencies.sh
if [[ "$TRAVIS_OS_NAME" == "windows" ]]; then 
    ./travis/get-windows-vcpkg-cache.sh
    ./travis/get-windows-d3d9sdk.sh
fi
if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then 
    ./travis/get-linux-vcpkg-cache.sh
fi 
if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then 
    ./travis/get-osx-sdk.sh $MACOSX_DEPLOYMENT_TARGET
    if [[ ! -f cache/libIrrlicht.a ]]; then
        ./travis/install-osx-dependencies.sh
    else
        cp -r cache/irrlicht /usr/local/include
        cp cache/libIrrlicht.a.a /usr/local/lib
        rm -rf /usr/local/Cellar/libevent/2.1.11
        cp -r cache/libevent /usr/local/Cellar/libevent/2.1.11
        rm -rf cache && echo "Loaded irrlicht and libevent from cache."
    fi
fi
