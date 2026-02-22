#!/bin/sh

# This script builds the C# libraries manually - Xcode currently will not build them so we need to do it ourselves.

CONFIG="Release"

if [ ! -z "$1" ]; then
    CONFIG="$1"
fi

projects=("Hyperion.NET.Shared" "Hyperion.NET.Runtime" "Hyperion.NET.Interop" "Hyperion.NET.Scripting")

pushd Build
buildDir="$(pwd)"
pushd CSharpProjects

for project in "${projects[@]}"; do
    mkdir -p "$buildDir/../Binaries/Engine"
    
    echo "Building $project in $CONFIG configuration..."

    pushd "$project"
        dotnet build --disable-build-servers --no-restore --configuration "$CONFIG"
        if [ $? -ne 0 ]; then
            echo "Failed to build $project"
            exit 1
        fi
        # # Copy the built DLL to the Binaries folder
        # if [ ! -d "$buildDir/../Binaries" ]; then
        #     mkdir -p "$buildDir/../Binaries"
        # fi
        # cp "bin/$CONFIG/net9.0/$project.dll" "$buildDir/../Binaries/$project.dll"
    popd # $project
done

popd # CSharpProjects
popd # Build
