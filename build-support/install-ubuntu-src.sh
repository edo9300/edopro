#!/usr/bin/env bash

# Installs needed dependencies to /usr/local from source
git clone --depth=1 --branch=master https://github.com/fmtlib/fmt.git /tmp/fmt
cd /tmp/fmt
cmake .
make
sudo make install

git clone --depth=1 --branch=master https://github.com/nlohmann/json.git /tmp/nlohmann-json
cd /tmp/nlohmann-json
cmake .
make
sudo make install
