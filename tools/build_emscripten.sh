#!/bin/bash

docker run --rm -v $(pwd):/src emscripten/emsdk \
    bash -c "mkdir -p build; cd build; emcmake cmake /src && emmake make; cd .."