#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Parse CLI args
IOS=0
IOS_SIMULATOR=0
XCODE=0
HYP_ANDROID=0
HYP_REGENERATE=0
HYP_NOWAIT=0

CONFIG="Release"

CURR_PLATFORM="$(uname -s)"
if [[ "$CURR_PLATFORM" == "Darwin" ]]; then
    CURR_PLATFORM="Mac"
fi

for arg in "$@"; do
    if [[ "$arg" == "IOS" ]]; then
        IOS=1
        CURR_PLATFORM="IOS"
    elif [[ "$arg" == "IOSSimulator" ]]; then
        IOS=1
        IOS_SIMULATOR=1
        CURR_PLATFORM="IOS"
    elif [[ "$arg" == "Xcode" ]]; then
        XCODE=1
    elif [[ "$arg" == "android" ]]; then
        HYP_ANDROID=1
        CURR_PLATFORM="Android"
    elif [[ "$arg" == "regenerate" ]]; then
        HYP_REGENERATE=1
    elif [[ "$arg" == "nowait" ]]; then
        HYP_NOWAIT=1
    fi

    if [[ "$arg" == "Debug" ]]; then
        CONFIG="Debug"
    elif [[ "$arg" == "Release" ]]; then
        CONFIG="Release"
    fi
done

if [[ $HYP_ANDROID -eq 1 ]]; then
    mkdir -p "Build/Android/$CONFIG"
    pushd "Build/Android/$CONFIG"
else
    mkdir -p "Build/$CURR_PLATFORM/$CONFIG"
    pushd "Build/$CURR_PLATFORM/$CONFIG"
fi

if [[ $HYP_REGENERATE -eq 1 ]]; then
    DO_CMAKE=1
elif [[ $HYP_NOWAIT -eq 1 ]]; then
    DO_CMAKE=0
else
    read -t 3 -p "Regenerate CMake? (will continue without regenerating in 3s) " RESP
    if [[ $RESP =~ ^[Yy] ]]; then
        DO_CMAKE=1
    else
        DO_CMAKE=0
    fi
fi

if [[ $DO_CMAKE -eq 1 ]]; then

    HYP_ROOT_DIR_ABS="$(realpath "$SCRIPT_DIR/../..")"
    printf "Using Hyperion root directory: %s\n" "$HYP_ROOT_DIR_ABS"

    HYP_CMAKE_PARAMS="-DHYP_THIRD_PARTY_LIBRARY_DIRECTORY=$SCRIPT_DIR/../../External/ThirdParty/Binaries -DHYP_LIBRARY_OUTPUT_DIRECTORY=$SCRIPT_DIR/../../Binaries -DHYP_RUNTIME_OUTPUT_DIRECTORY=$SCRIPT_DIR/../../Binaries -DHYP_ROOT_DIR=$HYP_ROOT_DIR_ABS"

    if [[ $HYP_ANDROID -eq 1 ]]; then
        if [[ -z "$ANDROID_NDK_HOME" ]]; then
            echo "ANDROID_NDK_HOME environment variable is not set. Please set it to the path of your Android NDK."
            exit 1
        fi
        ANDROID_NDK="$ANDROID_NDK_HOME"

        NINJA_EXE="$(which ninja 2>/dev/null)"
        if [[ -z "$NINJA_EXE" ]]; then
            echo "ERROR: Ninja not found. Make sure it is installed and in your PATH!"
            exit 1
        fi

        HOST_ARCH="$(uname -m)"
        if [[ "$(uname -s)" == "Darwin" ]]; then
            if [[ "$HOST_ARCH" == "arm64" ]]; then
                ANDROID_PREBUILT="darwin-arm64"
            else
                ANDROID_PREBUILT="darwin-x86_64"
            fi
        else
            ANDROID_PREBUILT="linux-x86_64"
        fi

        ANDROID_NDK_SYSROOT="$ANDROID_NDK/toolchains/llvm/prebuilt/$ANDROID_PREBUILT/sysroot"

        echo "Using Ninja: $NINJA_EXE"
        cmake ../../../Source -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA_EXE" \
            -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
            -DANDROID_ABI=arm64-v8a \
            -DANDROID_PLATFORM=android-28 \
            -DANDROID_STL=c++_shared \
            -DCMAKE_BUILD_TYPE="$CONFIG" \
            -DHYP_PLATFORM_NAME=Android \
            -DANDROID_NDK_SYSROOT="$ANDROID_NDK_SYSROOT" \
            $HYP_CMAKE_PARAMS

    elif [[ $IOS -eq 1 ]]; then
        if [[ -z "$VULKAN_SDK" ]]; then
            echo "VULKAN_SDK environment variable is not set. Please set it to the path of your Vulkan SDK."
            exit 1
        fi

        if [[ $XCODE -eq 1 ]]; then
            printf "Building with Xcode...\n"
            cmake -G Xcode ../../../Source -DHYP_PLATFORM_NAME=IOS -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_BUILD_TYPE="$CONFIG" $HYP_CMAKE_PARAMS
        else
            printf "Building with Ninja...\n"
            cmake -G Ninja ../../../Source -DHYP_PLATFORM_NAME=IOS -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 $HYP_CMAKE_PARAMS
        fi
    elif [[ $XCODE -eq 1 ]]; then
        printf "Building with Xcode...\n"
        cmake -G Xcode ../../../Source -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 $HYP_CMAKE_PARAMS
    else
        printf "Building with Ninja...\n"
        cmake -G Ninja ../../../Source $HYP_CMAKE_PARAMS
    fi
fi

cmake --build . --parallel 8 --config "$CONFIG"

popd
