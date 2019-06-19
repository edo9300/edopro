#!/usr/bin/env bash

# Installs needed binaries to the working directory
curl --retry 5 --connect-timeout 30 --location --remote-header-name --remote-name https://github.com/premake/premake-core/releases/download/v5.0.0-alpha14/premake-5.0.0-alpha14-windows.zip
rm -f premake5.exe
unzip -uo premake-5.0.0-alpha14-windows.zip
rm premake-5.0.0-alpha14-windows.zip

curl --retry 5 --connect-timeout 30 --location --remote-header-name --remote-name http://www.ambiera.at/downloads/irrKlang-64bit-1.6.0.zip
unzip -uo irrKlang-64bit-1.6.0.zip
rm -rf irrKlang
mv irrKlang-64bit-1.6.0 irrKlang
rm irrKlang-64bit-1.6.0.zip

# We wrap irrKlang32's extract because its zip has a hidden macOS directory that we don't want
curl --retry 5 --connect-timeout 30 --location --remote-header-name --remote-name https://www.ambiera.at/downloads/irrKlang-32bit-1.6.0.zip
unzip -uo irrKlang-32bit-1.6.0.zip -d irrKlang-tmp
# Merge 32-bit binaries into folder
mv irrKlang-tmp/irrKlang-1.6.0/bin/win32-gcc irrKlang/bin/win32-gcc
mv irrKlang-tmp/irrKlang-1.6.0/bin/win32-visualStudio irrKlang/bin/win32-visualStudio
mv irrKlang-tmp/irrKlang-1.6.0/lib/Win32-gcc irrKlang/lib/Win32-gcc
mv irrKlang-tmp/irrKlang-1.6.0/lib/Win32-visualStudio irrKlang/lib/Win32-visualStudio
rm -rf irrKlang-tmp
rm irrKlang-32bit-1.6.0.zip
