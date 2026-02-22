#!/bin/bash

# You need the NVIDIA Container Toolkit to run this script
# https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html

# To have this connect to a signalling server run it on your host machine on port 8000,
# Or alternatively, change the CLI args to point to a different server.
# The sample signalling server is found in tools/signalling_server, and you can run it with:
# python -m http.server 8000

docker run -it --rm \
    --name sample-streamer \
    --network host \
    --gpus all \
    --runtime=nvidia \
    --privileged \
    -e DISPLAY=$DISPLAY \
    -e NVIDIA_VISIBLE_DEVICES=all \
    -e NVIDIA_DRIVER_CAPABILITIES=all \
    sample-streamer:v1 \
    "--SignallingServerIP=ws://127.0.0.1" \
    "--SignallingServerPort=8000" \
    "--headless"