FROM ubuntu:20.04@sha256:ed4a42283d9943135ed87d4ee34e542f7f5ad9ecf2f244870e23122f703f91c2

ARG CMAKE_VERSION="3.26.4"

RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && \
    apt-get -y install \
        build-essential \
        curl \
        gdb \
        less \
        ninja-build \
        git 

RUN curl -L -o cmake.tgz https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-$(arch).tar.gz && \
    tar -xf cmake.tgz -C /opt && \
    mv /opt/cmake-${CMAKE_VERSION}-linux-$(arch) /opt/cmake-${CMAKE_VERSION}

ENV PATH=$PATH:/opt/cmake-${CMAKE_VERSION}/bin

CMD ["/bin/bash"]
