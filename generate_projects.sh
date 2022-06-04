#!/bin/sh
mkdir -p project
if [[ "$OSTYPE" == "darwin"* ]]; then
cmake -G Xcode
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
cmake -G Makefile
elif [[ "$OSTYPE" == "win32"* ]]; then
cmake -G "Visual Studio 22"
fi
