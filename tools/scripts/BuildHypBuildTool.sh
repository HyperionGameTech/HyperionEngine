#!/bin/bash

mkdir -p build/buildtool
pushd build
pushd buildtool

read -t 3 -p "Regenerate CMake? (will continue without regenerating in 3s) " RESP

if [[ $RESP =~ ^[Yy] ]]; then
    cmake ../../buildtool
fi

# Build the buildtool and move it to the build directory
cmake --build . --target hyperion-buildtool || exit 1

# find the hyperion-buildtool executable: will be in the folder on mac/linux, on windows will be under debug/release folder
if [ -f ./hyperion-buildtool ]; then
    echo "Found hyperion-buildtool in current directory"
elif [ -f ./Debug/hyperion-buildtool ]; then
    echo "Found hyperion-buildtool in Debug directory"
    mv ./Debug/hyperion-buildtool ..
elif [ -f ./Release/hyperion-buildtool ]; then
    echo "Found hyperion-buildtool in Release directory"
    mv ./Release/hyperion-buildtool ..
else
    echo "Could not find hyperion-buildtool executable"
    exit 1
fi

popd
popd


