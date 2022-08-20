#!/usr/bin/env bash

set -euxo pipefail
ARCH=${ARCH:-"x64"}

mkdir -p "$VCPKG_ROOT"
cd "$VCPKG_ROOT"
curl --retry 5 --connect-timeout 30 --location --remote-header-name --output installed.7z $VCPKG_CACHE_7Z_URL
7z x installed.7z
mkdir -p ./installed/$ARCH-osx/include/irrlicht/
mv irrlicht/include/* ./installed/$ARCH-osx/include/irrlicht
mv irrlicht/lib/* ./installed/$ARCH-osx/lib
