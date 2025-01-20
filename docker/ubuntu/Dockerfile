FROM ubuntu:24.04

LABEL authors="https://github.com/organicmaps"

SHELL ["/bin/bash", "-c"]

USER root

ENV CLANG_VERSION=18
ENV GCC_VERSION=14

RUN apt-get update -y
RUN apt-get install -y \
    cmake \
    ninja-build \
    libgl1-mesa-dev \
    libglvnd-dev \
    libharfbuzz-dev \
    qt6-base-dev \
    libqt6svg6-dev \
    qt6-positioning-dev \
    libqt6positioning6-plugins \
    libqt6positioning6 \
    ccache \
    clang-${CLANG_VERSION} \
    clang++-${CLANG_VERSION} \
    g++-${GCC_VERSION} \
    gcc-${GCC_VERSION} \
    python3 \
    python3-pip

RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-${CLANG_VERSION} 100
RUN update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-${CLANG_VERSION} 100
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_VERSION} 100
RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${GCC_VERSION} 100

ENV CMAKE_C_COMPILER_LAUNCHER=ccache
ENV CMAKE_CXX_COMPILER_LAUNCHER=ccache

COPY docker/ubuntu/env.sh /tmp
RUN cat "/tmp/env.sh" >> "${HOME}/.bashrc"
RUN rm /tmp/env.sh
