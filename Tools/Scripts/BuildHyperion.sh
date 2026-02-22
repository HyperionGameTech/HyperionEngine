#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

mkdir -p Build
pushd Build

# Parse CLI args
IOS=0
IOS_SIMULATOR=0
DARWIN=0
for arg in "$@"; do
    if [[ "$arg" == "--ios" ]]; then
        IOS=1
    elif [[ "$arg" == "--ios-simulator" ]]; then
        IOS=1
        IOS_SIMULATOR=1
    elif [[ "$arg" == "--xcode" ]]; then
        DARWIN=1
    fi
done

read -t 3 -p "Regenerate CMake? (will continue without regenerating in 3s) " RESP

if [[ $RESP =~ ^[Yy] ]]; then
    # Generate for iOS if requested
    if [[ $IOS -eq 1 ]]; then
        # Ensure VULKAN_SDK env var is set
        if [[ -z "$VULKAN_SDK" ]]; then
            echo "VULKAN_SDK environment variable is not set. Please set it to the path of your Vulkan SDK."
            exit 1
        fi

        HYP_ROOT_DIR_ABS="$(realpath "$SCRIPT_DIR/../..")"
        HYP_CMAKE_PARAMS="-DHYP_THIRD_PARTY_LIBRARY_DIRECTORY=$SCRIPT_DIR/../../Binaries/ThirdParty -DHYP_LIBRARY_OUTPUT_DIRECTORY=$SCRIPT_DIR/../../Binaries/Engine -DHYP_RUNTIME_OUTPUT_DIRECTORY=$SCRIPT_DIR/../../Binaries/Engine -DHYP_ROOT_DIR=$HYP_ROOT_DIR_ABS"

        if [[ $IOS_SIMULATOR -eq 1 ]]; then
            cmake -G Xcode ../Source -DHYP_PLATFORM_NAME=iOS -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 $HYP_CMAKE_PARAMS
        else
            cmake -G Xcode ../Source -DHYP_PLATFORM_NAME=iOS -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 $HYP_CMAKE_PARAMS
        fi
    elif [[ $DARWIN -eq 1 ]]; then
        cmake -G Xcode ../Source -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 $HYP_CMAKE_PARAMS
    else
        cmake ../Source $HYP_CMAKE_PARAMS
    fi
fi

cmake --build . --parallel 8

popd


