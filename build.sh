#!/usr/bin/env bash  

export PATH=/home/ubuntu/cuda:$PATH
export PATH=/home/ubuntu/libtorch:$PATH
rm -rf build && \
mkdir build && \
cd build && \
cmake ../ && \
make -j32