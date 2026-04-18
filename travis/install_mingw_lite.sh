#!/usr/bin/env bash

set -euo pipefail
MINGW_LITE_VARIANT=${1:-$MINGW_LITE_VARIANT}

cd /tmp

curl -L https://github.com/redpanda-cpp/mingw-lite/releases/download/15.2.0-r7/x-mingw32$MINGW_LITE_VARIANT-15.2.0-r7.tar.zst -o mingw.tar.zst

tar -xvf mingw.tar.zst

layers=(/tmp/mingw32$MINGW_LITE_VARIANT-15/AAB/{binutils,crt,gcc,headers}/usr/local)
lowerdir=$(IFS=:; echo "${layers[*]}")
sudo mount -t overlay none /usr/local -o lowerdir=$lowerdir