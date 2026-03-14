#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Parse CLI args
IOS=0
IOS_SIMULATOR=0
XCODE=0

CONFIG="Release"

CURR_PLATFORM="$(uname -s)"
# if Darwin we want to use "Mac" instead of that
if [[ "$CURR_PLATFORM" == "Darwin" ]]; then
    CURR_PLATFORM="Mac"
fi

# We currently don't support building Android from this script, would be nice to get there eventually but for now,
# just handle iOS as a specific platform arg that can be provided to override the default platform detection based on the host OS.

for arg in "$@"; do
    if [[ "$arg" == "iOS" ]]; then
        IOS=1
        CURR_PLATFORM="iOS"
    elif [[ "$arg" == "iOSSimulator" ]]; then
        IOS=1
        IOS_SIMULATOR=1
        CURR_PLATFORM="iOS"
    elif [[ "$arg" == "Xcode" ]]; then
        XCODE=1
    fi
    
    # config mode
    if [[ "$arg" == "Debug" ]]; then
        CONFIG="Debug"
    elif [[ "$arg" == "Release" ]]; then
        CONFIG="Release"
    fi
done

mkdir -p "Build/$CURR_PLATFORM/$CONFIG"
pushd "Build/$CURR_PLATFORM/$CONFIG"

read -t 3 -p "Regenerate CMake? (will continue without regenerating in 3s) " RESP

if [[ $RESP =~ ^[Yy] ]]; then

    HYP_ROOT_DIR_ABS="$(realpath "$SCRIPT_DIR/../..")"
    printf "Using Hyperion root directory: %s\n" "$HYP_ROOT_DIR_ABS"

    HYP_CMAKE_PARAMS="-DHYP_THIRD_PARTY_LIBRARY_DIRECTORY=$SCRIPT_DIR/../../../External/ThirdParty/Binaries -DHYP_LIBRARY_OUTPUT_DIRECTORY=$SCRIPT_DIR/../../../Binaries -DHYP_RUNTIME_OUTPUT_DIRECTORY=$SCRIPT_DIR/../../../Binaries -DHYP_ROOT_DIR=$HYP_ROOT_DIR_ABS"

    # Generate for iOS if requested
    if [[ $IOS -eq 1 ]]; then
        # Ensure VULKAN_SDK env var is set
        if [[ -z "$VULKAN_SDK" ]]; then
            echo "VULKAN_SDK environment variable is not set. Please set it to the path of your Vulkan SDK."
            exit 1
        fi

        if [[ $IOS_SIMULATOR -eq 1 ]]; then
            cmake -G Xcode ../../../Source -DHYP_PLATFORM_NAME=iOS -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 $HYP_CMAKE_PARAMS
        else
            cmake -G Xcode ../../../Source -DHYP_PLATFORM_NAME=iOS -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 $HYP_CMAKE_PARAMS
        fi
    elif [[ $XCODE -eq 1 ]]; then
        cmake -G Xcode ../../../Source -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 $HYP_CMAKE_PARAMS
    else
        cmake ../../../Source $HYP_CMAKE_PARAMS
    fi
fi

cmake --build . --parallel 8

popd


