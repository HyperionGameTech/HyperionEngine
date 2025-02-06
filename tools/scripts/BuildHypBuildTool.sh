#!/bin/bash

mkdir -p build/buildtool
pushd build
pushd buildtool

read -t 3 -p "Regenerate CMake? (will continue without regenerating in 3s) " RESP

if [[ $RESP =~ ^[Yy] ]]; then
    cmake ../../buildtool
fi

# Build the buildtool and move it to the build directory
cmake --build . --target hyperion-buildtool && mv ./hyperion-buildtool ..

popd
popd


