FROM ubuntu:16.04

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends apt-utils && \
    apt-get -y upgrade && \
    apt-get install -y build-essential git vim software-properties-common

RUN add-apt-repository -y ppa:ubuntu-toolchain-r/test && \
    apt-get update && \
    apt-get install -y gcc-6 g++-6

RUN apt-get install -y pkg-config libfuse-dev fuse && \
    git clone https://github.com/gittup/tup.git && \
    cd tup && \
    CFLAGS="-g" ./build.sh

ADD . /lift

ENTRYPOINT cd tup && \
    ./build/tup init && \
    ./build/tup upd && \
    cp tup /usr/bin/ && \
    cd .. && rm -rf tup && \
    echo done && /bin/bash

# build: sudo docker build .
# run: docker run -i -t <image> /bin/bash
