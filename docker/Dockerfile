FROM nvidia/cuda:10.2-cudnn8-devel-centos7

# Prepare and empty machine for building
RUN yum update -y

RUN yum install -y epel-release
RUN yum install -y \
    git \
    cmake3 \
    make \
    gcc-c++ \
    wget \
    unzip

# Install libtorch
RUN wget https://download.pytorch.org/libtorch/cu101/libtorch-cxx11-abi-shared-with-deps-1.7.1%2Bcu101.zip
RUN unzip libtorch-cxx11-abi-shared-with-deps-1.7.1+cu101.zip

# Install openCV
RUN yum install -y opencv opencv-devel opencv-python

# Install LIBZIP
RUN wget https://libzip.org/download/libzip-1.7.3.tar.gz
RUN tar -xzvf libzip-1.7.3.tar.gz
RUN cd libzip-1.7.3 && \
    mkdir build && \
    cd build && \
    cmake3 ../ && \
    make && \
    make test && \
    make install 

# Install Pangolin
RUN git clone https://github.com/stevenlovegrove/Pangolin.git
RUN cd Pangolin && \
    mkdir build && \
    cd build && \
    cmake3 ../ && \
    cmake3 --build .

# Add libtorch to PATH
ENV PATH="/working/libtorch:${PATH}"

# Install GCC v8
RUN yum install centos-release-scl
RUN yum install devtoolset-8-gcc devtoolset-8-gcc-c++
RUN scl enable devtoolset-8 -- bash

# Install boost
RUN yum install -y boost-devel

# Build and install deepDSO
RUN git clone https://github.com/Hivemapper/deepDSO.git

# Install deepDSO
RUN cd deepDSO && \
    git checkout dev && \
    mkdir build
RUN	cd build && \
    cmake3 && \
    make -j4 &&