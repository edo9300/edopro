#!/usr/bin/env bash

set -euxo pipefail
ARCH=${ARCH:-"x64"}
TARGET_OS=${TARGET_OS:-$TRAVIS_OS_NAME}

./travis/install-local-dependencies.sh
if [[ "$TARGET_OS" == "windows" ]]; then
    ./travis/get-windows-vcpkg-cache.sh
    ./travis/get-windows-d3d9sdk.sh
else
    ./travis/get-nix-vcpkg-cache.sh
fi
