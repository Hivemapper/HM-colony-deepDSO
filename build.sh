#!/usr/bin/env bash  

export PATH=/home/ubuntu/cuda:$PATH
export PATH=/home/ubuntu/libtorch:$PATH
cd build && \
cmake ../ && \
make -j32