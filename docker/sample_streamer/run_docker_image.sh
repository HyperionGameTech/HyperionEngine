#!/bin/bash

docker run -it --rm \
    --name sample-streamer \
    --network host \
    -v "--SignallingServerIP=ws://127.0.0.1" \
    -v "--SignallingServerPort=8000" \
    sample-streamer:v1