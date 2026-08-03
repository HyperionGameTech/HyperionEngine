#!/bin/bash

# Parse CLI arguments for build tool version
if [ $# -lt 2 ]; then
    echo "Usage: $0 <major> <minor>"
    exit 1
fi

HYP_CODEGEN_VERSION_MAJOR=$1
HYP_CODEGEN_VERSION_MINOR=$2

SCRIPT_DIR=`dirname -- "$( readlink -f -- "$0"; )"`
WORKING_DIR=`pwd`

# Version-based rebuild logic
REBUILD=false
INC_FILE="$WORKING_DIR/Source/Generated/CodeGenOutput.inc"

# Check if both the build tool and version file exist
if [ ! -f ./build/hyperion-codegen ]; then
    echo "Hyperion build tool not found. Running BuildCodeGen.sh ..."
    REBUILD=true
else
    if [ ! -f "$INC_FILE" ]; then
        echo "Version file not found. Running BuildCodeGen.sh ..."
        REBUILD=true
    else
        # Read and check version
        LINE=$(head -n 1 "$INC_FILE")
        
        if [ -z "$LINE" ]; then
            echo "Version not found in file. Running BuildCodeGen.sh ..."
            REBUILD=true
        else
            # expect "//! VERSION major.minor.patch"
            VERSION_PATTERN="//! VERSION ([0-9]+)\.([0-9]+)"
            if [[ $LINE =~ $VERSION_PATTERN ]]; then
                FILE_MAJOR="${BASH_REMATCH[1]}"
                FILE_MINOR="${BASH_REMATCH[2]}"
                
                if [ "$FILE_MAJOR" -ne "$HYP_CODEGEN_VERSION_MAJOR" ]; then
                    REBUILD=true
                    REBUILD_REASON="Major version mismatch"
                fi
                
                if [ "$FILE_MINOR" -ne "$HYP_CODEGEN_VERSION_MINOR" ]; then
                    REBUILD=true
                    REBUILD_REASON="Minor version mismatch"
                fi
                
                if [ "$REBUILD" = true ]; then
                    echo "Rebuild needed: $REBUILD_REASON (file: $FILE_MAJOR.$FILE_MINOR, expected: $HYP_CODEGEN_VERSION_MAJOR.$HYP_CODEGEN_VERSION_MINOR)"
                fi
            else
                echo "Invalid version format. Running BuildCodeGen.sh ..."
                REBUILD=true
            fi
        fi
    fi
fi

if [ "$REBUILD" = true ]; then
    echo "Running BuildCodeGen.sh ..."
    if (printf "y" | ./Tools/Scripts/BuildCodeGen.sh); then
        # Check if build tool was created
        if [ ! -f ./Build/hyperion-codegen ]; then
            echo "Build tool returned success, but the executable could not be found!"
            exit 1
        fi
    else
        echo "Failed to build Hyperion build tool."
        exit 1
    fi
else
    echo "Build tool is up-to-date, skipping rebuild."
fi

# Run the build tool
./Build/hyperion-codegen --WorkingDirectory=$WORKING_DIR --SourceDirectory=$WORKING_DIR/Source --CXXOutputDirectory=$WORKING_DIR/Source/Generated --CSharpOutputDirectory=$WORKING_DIR/Source/Generated/CSharp --HypScriptOutputDirectory=$WORKING_DIR/Data/Scripts --StrataOutputDirectory=$WORKING_DIR/Data/Scripts/Strata --ExcludeDirectories=$WORKING_DIR/Source/Generated --ExcludeFiles=$WORKING_DIR/Source/Core/Defines.hpp