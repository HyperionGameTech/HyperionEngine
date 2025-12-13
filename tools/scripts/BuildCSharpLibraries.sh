#!/bin/sh

# This script builds the C# libraries manually - Xcode currently will not build them so we need to do it ourselves.

CONFIG="Release"

if [ ! -z "$1" ]; then
    CONFIG="$1"
fi

projects=("Hyperion.NET.Shared" "Hyperion.NET.Runtime" "Hyperion.NET.Interop" "Hyperion.NET.Scripting")

pushd build
buildDir="$(pwd)"
pushd CSharpProjects

for project in "${projects[@]}"; do
    echo "Building $project..."

    pushd "$project"
        dotnet build --disable-build-servers --no-restore --configuration "$CONFIG"
        if [ $? -ne 0 ]; then
            echo "Failed to build $project"
            exit 1
        fi
        mkdir -p "$buildDir/bin"
        dstPath="$buildDir/bin/$project.dll"
        srcPath="bin/$CONFIG/net9.0/$project.dll"
        echo "copy $srcPath to $dstPath"
        cp "$srcPath" "$dstPath"
    popd # $project
done

popd # CSharpProjects
popd # build
