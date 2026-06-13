#!/usr/bin/env bash

set -euo pipefail
MINGW_LITE_VARIANT=${1:-$MINGW_LITE_VARIANT}

cd /tmp

curl -L https://github.com/redpanda-cpp/mingw-lite/releases/download/16.1.0-r0/x-mingw32$MINGW_LITE_VARIANT-16.1.0-r0.tar.zst -o mingw.tar.zst

tar -xvf mingw.tar.zst

layers=(/tmp/mingw32$MINGW_LITE_VARIANT-16/AAB/{binutils,crt-target,gcc,gcc-lib,headers,mcfgthread,winpthreads}/usr/local)
lowerdir=$(IFS=:; echo "${layers[*]}")
sudo mount -t overlay none /usr/local -o lowerdir=$lowerdir
