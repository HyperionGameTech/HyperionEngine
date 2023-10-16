#!/bin/bash

docker run --rm -v $(pwd):/src emscripten/emsdk \
    bash -c "mkdir -p build/emscripten; cd build/emscripten; emcmake cmake /src && emmake make; cd ../.."