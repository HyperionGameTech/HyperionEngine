#!/bin/bash

SCRIPT_DIR=`dirname -- "$( readlink -f -- "$0"; )"`

read -t 3 -p "Regenerate CMake? (will continue without regenerating in 3s) " RESP

if [[ $RESP =~ ^[Yy] ]]; then
    (printf "y" | ./tools/scripts/BuildHyperion.sh "$@")
else
    (printf "n" | ./tools/scripts/BuildHyperion.sh "$@")
fi

