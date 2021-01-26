FROM nvidia/cuda:10.2-cudnn8-devel-centos8

# Prepare and empty machine for building
RUN yum update -y
RUN yum install -y \
    git \
    cmake3 \
    make \
    wget \
    unzip \
    gcc

# Install openCV
RUN git clone https://github.com/opencv/opencv.git
RUN cd opencv && \
    git checkout tags/4.5.1 && \
    mkdir build && \
    cd build && \
    cmake3 ../ && \
    make -j8 && \
    make install

# Install LIBZIP
RUN dnf install -y libzip libzip-devel

# Install GLEW
RUN dnf --enablerepo=powertools install -y glew-devel

# Install Pangolin
RUN yum install -y mesa-libGL mesa-libGL-devel
RUN git clone https://github.com/stevenlovegrove/Pangolin.git
RUN cd Pangolin && \
    git checkout 86eb4975fc4fc8b5d92148c2e370045ae9bf9f5d && \
    mkdir build && \
    cd build && \
    cmake3 ../ && \
    cmake3 --build .

# Install libtorch
RUN wget https://download.pytorch.org/libtorch/cu101/libtorch-cxx11-abi-shared-with-deps-1.7.1%2Bcu101.zip
RUN unzip libtorch-cxx11-abi-shared-with-deps-1.7.1+cu101.zip
ENV PATH="/libtorch:$PATH"

# Install dependencies
RUN yum install -y boost-devel
RUN dnf --enablerepo=powertools install -y suitesparse-devel eigen3-devel zlib-devel openssl-devel

# Install cmake 3.19.3
RUN wget https://github.com/Kitware/CMake/releases/download/v3.19.3/cmake-3.19.3.tar.gz
RUN tar -xzvf cmake-3.19.3.tar.gz
RUN cd cmake-3.19.3 && \
    ./bootstrap && \
    make -j16 && \
    make install

# Build and install deepDSO
RUN git clone https://github.com/Hivemapper/deepDSO.git && \
    cd deepDSO && \
    git checkout dev && \
    mkdir build && \
    cd build && \
    cmake3 ../ && \
    make -j16