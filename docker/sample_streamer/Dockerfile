FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    make \
    cmake \
    libvulkan-dev \
    libvulkan1 \
    libsdl2-dev \
    libssl-dev \
    libopenal-dev \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav

ENV CC=gcc
ENV CXX=g++

WORKDIR /app

COPY ./src ./src
COPY ./include ./include
COPY ./res/shaders ./res/shaders
COPY ./res/scripts ./res/scripts
COPY ./res/models/gaussian_splatting/ ./res/models/gaussian_splatting
COPY ./samples ./samples
COPY ./submodules ./submodules
COPY ./build.sh ./build.sh
COPY ./CMakeLists.txt ./CMakeLists.txt
RUN set -e; \
    mkdir -p build; \
    cd build; \
    cmake ..; \
    make -j4; \
    cd ..; 

RUN rm -rf src include samples submodules build.sh CMakeLists.txt
RUN apt-get remove -y g++ make cmake

CMD ["./build/samples/hyperion-sample", "--SignallingServerIP=ws://127.0.0.1", "--SignallingServerPort=8000"]
