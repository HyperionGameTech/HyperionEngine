#!/bin/bash

docker run -it --rm \
    --name sample-streamer \
    --network host \
    -e SIGNALLING_SERVER_IP="ws://127.0.0.1" \
    -e SIGNALLING_SERVER_PORT="8000" \
    sample-streamer:v1