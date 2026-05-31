#!/bin/sh

# This script builds the C# libraries manually - Xcode currently will not build them so we need to do it ourselves.

CONFIG="Release"

CURR_PLATFORM="$(uname -s)"
# if Darwin we want to use "Mac" instead of that
if [[ "$CURR_PLATFORM" == "Darwin" ]]; then
    CURR_PLATFORM="Mac"
fi

if [ ! -z "$1" ]; then
    CONFIG="$1"
fi

projects=("Hyperion.NET.Shared" "Hyperion.NET.Runtime" "Hyperion.NET.Interop" "Hyperion.NET.Scripting" "Hyperion.Editor")

pushd Build
buildDir="$(pwd)"
pushd CSharpProjects

for project in "${projects[@]}"; do
    mkdir -p "$buildDir/../Binaries/$CURR_PLATFORM/$CONFIG"

    echo "Building $project in $CONFIG configuration..."

    pushd "$project"
        dotnet build --disable-build-servers --no-restore --configuration "$CONFIG"
        if [ $? -ne 0 ]; then
            echo "Failed to build $project"
            exit 1
        fi
        # Copy the built DLL to the Binaries folder
        if [ ! -d "$buildDir/../Binaries/$CURR_PLATFORM/$CONFIG" ]; then
            mkdir -p "$buildDir/../Binaries/$CURR_PLATFORM/$CONFIG"
        fi
        cp "bin/$CONFIG/net9.0/$project.dll" "$buildDir/../Binaries/$CURR_PLATFORM/$CONFIG/$project.dll"
    popd # $project
done

popd # CSharpProjects
popd # Build
