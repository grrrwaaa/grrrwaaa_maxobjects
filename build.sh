#!/bin/sh

# Note that LuaJIT here is built 32-bit, because there is currently no way to 
# embed the 64-bit version in Max on OSX.

ROOT=`pwd`

git submodule init && git submodule update

# build LuaJIT:
cd luajit-2.0
make CC="gcc -m32" && make install PREFIX=$ROOT/luajit_osx
cd ..
# only ever link the static lib:
rm $ROOT/luajit_osx/lib/*.dylib