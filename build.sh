#!/bin/bash

SCRIPT_DIR=`dirname -- "$( readlink -f -- "$0"; )"`

BUILD_TOOL_CMD="./build/hyperion-buildtool --WorkingDirectory=$SCRIPT_DIR --SourceDirectory=$SCRIPT_DIR/src --CXXOutputDirectory=$SCRIPT_DIR/build/generated --CSharpOutputDirectory=$SCRIPT_DIR/src/dotnet/runtime/gen --ExcludeDirectories=$SCRIPT_DIR/src/generated --ExcludeFiles=$SCRIPT_DIR/src/core/Defines.hpp"

read -t 3 -p "Regenerate CMake? (will continue without regenerating in 3s) " RESP

if [[ $RESP =~ ^[Yy] ]]; then
    (printf "y" | ./tools/scripts/BuildHyperion.sh "$@")
else
    (printf "n" | ./tools/scripts/BuildHyperion.sh "$@")
fi

