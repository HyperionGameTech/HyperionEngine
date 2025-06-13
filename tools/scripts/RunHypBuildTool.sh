#!/bin/bash

SCRIPT_DIR=`dirname -- "$( readlink -f -- "$0"; )"`

# check if build/hyperion-buildtool exists
if [ ! -f ./build/hyperion-buildtool ]; then
    echo "Hyperion build tool not found. Running BuildHypBuildTool.sh ..."
    
    if (printf "y" | ./tools/scripts/BuildHypBuildTool.sh); then
        echo "Hyperion build tool built successfully."
    else
        echo "Failed to build Hyperion build tool."
        exit 1
    fi
fi

WORKING_DIR=`pwd`
./build/hyperion-buildtool --WorkingDirectory=$WORKING_DIR --SourceDirectory=$WORKING_DIR/src --CXXOutputDirectory=$WORKING_DIR/build/generated --CSharpOutputDirectory=$WORKING_DIR/build/generated/dotnet/runtime --ExcludeDirectories=$WORKING_DIR/src/generated --ExcludeFiles=$WORKING_DIR/src/core/Defines.hpp