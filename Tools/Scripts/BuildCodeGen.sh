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
    EXE_DIR="."
elif [ -f ./Debug/hyperion-codegen ]; then
    echo "Found hyperion-codegen in Debug directory"
    mv ./Debug/hyperion-codegen ..
    EXE_DIR="./Debug"
elif [ -f ./Release/hyperion-codegen ]; then
    echo "Found hyperion-codegen in Release directory"
    mv ./Release/hyperion-codegen ..
    EXE_DIR="./Release"
else
    echo "Could not find hyperion-codegen executable"
    exit 1
fi

# Move the hyperion-core shared library alongside the executable
CORE_LIB=""
for lib in "$EXE_DIR/libhyperion-core.so" "$EXE_DIR/libhyperion-core.dylib"; do
    if [ -f "$lib" ]; then
        CORE_LIB="$lib"
        break
    fi
done

if [ -n "$CORE_LIB" ]; then
    echo "Moving $CORE_LIB .."
    mv "$CORE_LIB" ..
else
    echo "Could not find hyperion-core shared library - executable may fail to launch!"
fi

popd


