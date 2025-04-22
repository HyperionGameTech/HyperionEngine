#!/bin/bash

mkdir -p build
pushd build

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
    elif [[ "$arg" == "--darwin" ]]; then
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

        if [[ $IOS_SIMULATOR -eq 1 ]]; then
            cmake -G Xcode .. -DHYP_PLATFORM_NAME=iOS -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
        else
            cmake -G Xcode .. -DHYP_PLATFORM_NAME=iOS -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
        fi
    elif [[ $DARWIN -eq 1 ]]; then
        cmake -G Xcode ..
    else
        cmake ..
    fi
fi

cmake --build . --parallel 8

popd


