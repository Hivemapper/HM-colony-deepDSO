FROM nvidia/cuda:10.1-cudnn8-devel-centos8

# Install dependencies
RUN yum install -y \
    git \
    cmake3 \
    make \
    wget \
    unzip \
    gcc \
    boost-devel \
    mesa-libGL-devel
RUN dnf install -y libzip-devel && \
    dnf --enablerepo=PowerTools install -y suitesparse-devel \
    eigen3-devel \
    zlib-devel \
    openssl-devel \
    glew-devel

# Install cmake 3.19.3
RUN wget https://github.com/Kitware/CMake/releases/download/v3.19.3/cmake-3.19.3.tar.gz && \
    tar -xzvf cmake-3.19.3.tar.gz && \
    cd cmake-3.19.3 && \
    ./bootstrap && \
    make -j4 && \
    make install

# Install libtorch
RUN wget https://download.pytorch.org/libtorch/cu101/libtorch-cxx11-abi-shared-with-deps-1.7.1%2Bcu101.zip && \
    unzip libtorch-cxx11-abi-shared-with-deps-1.7.1+cu101.zip
ENV PATH="/libtorch:$PATH"

# Install openCV
RUN git clone https://github.com/opencv/opencv.git && \
    cd opencv && \
    git checkout tags/4.5.1 && \
    mkdir build && \
    cd build && \
    cmake3 ../ && \
    make && \
    make install

# Install Pangolin
RUN git clone https://github.com/stevenlovegrove/Pangolin.git && \
    cd Pangolin && \
    git checkout 86eb4975fc4fc8b5d92148c2e370045ae9bf9f5d && \
    mkdir build && \
    cd build && \
    cmake3 ../ && \
    cmake3 --build .

# Build and install deepDSO
RUN git clone https://github.com/Hivemapper/deepDSO.git && \
    cd deepDSO && \
    git checkout dev && \
    mkdir build && \
    cd build && \
    cmake3 ../ && \
    make
