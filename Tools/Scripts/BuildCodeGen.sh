#!/bin/bash

mkdir -p ./Build/CodeGen
pushd ./Build/CodeGen

read -t 3 -p "Regenerate CMake? (will continue without regenerating in 3s) " RESP

if [[ $RESP =~ ^[Yy] ]]; then
    cmake ../../Tools/CodeGen
fi

# Build the codegen and move it to the build directory
cmake --build . --target hyperion-codegen --parallel 4 || exit 1

# find the hyperion-codegen executable: will be in the folder on mac/linux, on windows will be under debug/release folder
if [ -f ./hyperion-codegen ]; then
    echo "Found hyperion-codegen in current directory"
    mv ./hyperion-codegen ..
elif [ -f ./Debug/hyperion-codegen ]; then
    echo "Found hyperion-codegen in Debug directory"
    mv ./Debug/hyperion-codegen ..
elif [ -f ./Release/hyperion-codegen ]; then
    echo "Found hyperion-codegen in Release directory"
    mv ./Release/hyperion-codegen ..
else
    echo "Could not find hyperion-codegen executable"
    exit 1
fi

popd


