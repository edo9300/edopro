#!/usr/bin/env bash

if [[ ! $VCPKG_ROOT ]]; then
    export VCPKG_ROOT=/c/vcpkg
    git clone --depth=1 --branch=lua-c++ https://github.com/kevinlul/vcpkg.git $VCPKG_ROOT
    #git clone https://github.com/Microsoft/vcpkg.git $VCPKG_ROOT
    cd $VCPKG_ROOT
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '.\bootstrap-vcpkg.bat'"
    ./vcpkg.exe integrate install
    ./vcpkg.exe integrate powershell # optional
    cd -
fi
export VCPKG_DEFAULT_TRIPLET=${1:-x86-windows-static}
$VCPKG_ROOT/vcpkg.exe install freetype libevent lua[cpp] sqlite3 fmt curl libgit2 nlohmann-json