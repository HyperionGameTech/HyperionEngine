FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    make \
    cmake \
    libvulkan-dev \
    libvulkan1 \
    libssl-dev \
    libopenal-dev \
    wget \
    xorg-dev\
    libudev-dev \ 
    libasound2-dev \
    libpulse-dev \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxi-dev \
    libxss-dev \
    libxcursor-dev \
    libxinerama-dev \
    libgl1-mesa-dev \
    libdbus-1-dev \
    libsndfile1-dev \
    libmodplug-dev \
    libgstreamer1.0-0 \
    libgstreamer1.0-dev \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav

ENV CC=gcc
ENV CXX=g++

# Compile SDL2 from source
WORKDIR /tmp
RUN wget https://libsdl.org/release/SDL2-2.0.20.tar.gz \
    && tar -zxvf SDL2-2.0.20.tar.gz
WORKDIR /tmp/SDL2-2.0.20
RUN ./configure \
    && make \
    && make install
# Cleanup SDL2 source
RUN rm -rf /tmp/SDL2-2.0.20.tar.gz /tmp/SDL2-2.0.20

WORKDIR /app

ENV SIGNALLING_SERVER_IP=ws://127.0.0.1
ENV SIGNALLING_SERVER_PORT=8000

COPY ./src ./src
COPY ./include ./include
COPY ./res/shaders ./res/shaders
COPY ./res/scripts ./res/scripts
COPY ./res/*.def ./res/
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
    cd ..; \
    mv ./build/samples/hyperion-sample /app/hyperion-sample; 

RUN rm -rf src include samples submodules build.sh CMakeLists.txt build/*
RUN apt-get remove -y g++ make cmake

# Precompile shaders for faster startup
RUN ./hyperion-sample --mode=precompile_shaders

ENTRYPOINT ["./hyperion-sample"]