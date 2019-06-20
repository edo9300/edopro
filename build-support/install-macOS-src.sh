#!/usr/bin/env bash

# Installs lua for C++ to /usr/local from source because brew's version is wonky
cd /tmp
curl --retry 5 --connect-timeout 30 --location --remote-header-name --remote-name https://www.lua.org/ftp/lua-5.3.5.tar.gz
tar xf lua-5.3.5.tar.gz
cd lua-5.3.5
make macosx CC=g++ install
