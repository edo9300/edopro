#!/usr/bin/env bash

sudo apt-get install build-essential p7zip-full libevent-dev libfmt-dev libfreetype6-dev libirrlicht-dev liblua5.3-dev libsqlite3-dev libgl1-mesa-dev libglu-dev libcurl4-openssl-dev libgit2-dev libasound2 -y

# nlohmann-json3-dev is only available in the latest Ubuntu distro's repo for some reason
sudo add-apt-repository 'deb http://archive.ubuntu.com/ubuntu/ eoan universe'
sudo apt-get install nlohmann-json3-dev -y
# delete it after because it overrides our other repos
sudo add-apt-repository --remove 'deb http://archive.ubuntu.com/ubuntu/ eoan universe'

curl --retry 5 --connect-timeout 30 --location --remote-header-name --remote-name https://github.com/premake/premake-core/releases/download/v5.0.0-alpha14/premake-5.0.0-alpha14-linux.tar.gz
rm -f premake5
tar xf premake-5.0.0-alpha14-linux.tar.gz
rm premake-5.0.0-alpha14-linux.tar.gz

curl --retry 5 --connect-timeout 30 --location --remote-header-name --remote-name http://www.ambiera.at/downloads/irrKlang-64bit-1.6.0.zip
7z x irrKlang-64bit-1.6.0.zip
rm -rf irrKlang
mv irrKlang-64bit-1.6.0 irrKlang
rm irrKlang-64bit-1.6.0.zip

mkdir -p bin
mkdir -p bin/debug
mkdir -p bin/release
cp -f irrKlang/bin/linux-gcc-64/libIrrKlang.so bin/debug/libIrrKlang.so
cp -f irrKlang/bin/linux-gcc-64/libIrrKlang.so bin/release/libIrrKlang.so
