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

# # Install mbedtls-devel
# RUN wget https://download-ib01.fedoraproject.org/pub/epel/8/Everything/x86_64/Packages/e/epel-release-8-10.el8.noarch.rpm
# RUN rpm -Uvh epel-release*rpm
# RUN dnf install -y mbedtls-devel

# # Install LIBZIP
RUN dnf install -y libzip libzip-devel
# RUN yum install -y nettle-devel gnutls-devel openssl-devel bzip2-devel mbedtls-devel
# RUN wget https://libzip.org/download/libzip-1.7.3.tar.gz
# RUN tar -xzvf libzip-1.7.3.tar.gz
# RUN cd libzip-1.7.3 && \
#     mkdir build && \
#     cd build && \
#     cmake3 ../ && \
#     make && \
#     make test && \
#     make install 

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

# Install boost
RUN yum install -y boost-devel
# Install suitesparse
RUN dnf --enablerepo=powertools install -y suitesparse-devel

# # Install Protobuf
# RUN git clone https://github.com/protocolbuffers/protobuf.git
# RUN cd protobuf && \
#     git submodule update --init --recursive && \
#     ./autogen.sh && \
#     ./configure && \
#     make -j16 && \
#     make check && \
#     sudo make install && \
#     sudo ldconfig # refresh shared library cache.

# Install dependencies
RUN dnf --enablerepo=powertools install -y eigen3-devel zlib-devel

# # Build and install deepDSO
# RUN git clone https://github.com/Hivemapper/deepDSO.git && \
#     cd deepDSO && \
#     git checkout dev && \
#     mkdir build && \
#     cd build && \
#     cmake3 ../ && \
#     make -j4