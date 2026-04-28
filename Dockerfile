## Build stage
FROM ubuntu:18.04

ARG VER=latest
ARG BUILD_JOBS=2

RUN apt update && apt install -y git

WORKDIR /amax.meta.chain
COPY . /amax.meta.chain

RUN JOBS=${BUILD_JOBS} bash ./scripts/amax_build.sh -y
RUN cd ./build/packages && bash generate_package.sh deb

## Runtime stage
FROM phusion/baseimage:bionic-1.0.0

ARG PUB_KEY
ARG PRIV_KEY
ARG VER=latest
ARG amc_pkg="amax_${VER}-1_amd64.deb"

COPY --from=0 /amax.meta.chain/build/packages/${amc_pkg} /${amc_pkg}

RUN apt-get update && apt-get upgrade -y && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libicu60 libusb-1.0-0 libcurl3-gnutls

RUN apt -y install /${amc_pkg}
RUN rm -rf /${amc_pkg} && mkdir /opt/amax

WORKDIR /opt/amax
