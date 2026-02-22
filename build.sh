#!/bin/bash

SCRIPT_DIR=`dirname -- "$( readlink -f -- "$0"; )"`

read -t 3 -p "Regenerate CMake? (will continue without regenerating in 3s) " RESP

if [[ $RESP =~ ^[Yy] ]]; then
    (printf "y" | ./Tools/Scripts/BuildHyperion.sh "$@")
else
    (printf "n" | ./Tools/Scripts/BuildHyperion.sh "$@")
fi

