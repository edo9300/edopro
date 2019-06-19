#!/usr/bin/env bash

curl --retry 5 --connect-timeout 30 --location --remote-header-name --remote-name https://github.com/premake/premake-core/releases/download/v5.0.0-alpha14/premake-5.0.0-alpha14-macosx.tar.gz
rm -f premake5
tar xf premake-5.0.0-alpha14-macosx.tar.gz
rm premake-5.0.0-alpha14-macosx.tar.gz

curl --retry 5 --connect-timeout 30 --location --remote-header-name --remote-name http://www.ambiera.at/downloads/irrKlang-64bit-1.6.0.zip
7z x irrKlang-64bit-1.6.0.zip
rm -rf irrKlang
mv irrKlang-64bit-1.6.0 irrKlang
rm irrKlang-64bit-1.6.0.zip
# For convenience with dylibbundler
cp -f irrKlang/bin/macosx-gcc/libirrklang.dylib /usr/local/lib