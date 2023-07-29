#!/bin/bash
mkdir -p build
pushd build

read -t 3 -p "Regenerate CMake? (will continue without regenerating in 3s) " RESP

if [[ $? -lt 128 ]] ; then
    case $RESP in
        [Yy]* ) cmake ../; break;;
        [Nn]* ) break;;
        * ) break;;
    esac
fi

cmake --build . -j 4
popd


