#!/bin/sh
mkdir -p project

if [[ "$OSTYPE" == "darwin"* ]]; then
# change to correct path if need be
cmake -GXcode -B./project -D CMAKE_CXX_COMPILER="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++" -D CMAKE_C_COMPILER="/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/cc"
else
echo "No project to generate (or not added to this script yet), instead use ./build.sh"
fi
