# This Dockerfile creates a docker image suitable to build a debian package

FROM docker.io/debian:sid

RUN apt update

# Needed to build biboumi
RUN apt install -y \
    git \
    devscripts
