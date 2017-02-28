#!/bin/sh

if [[ -z "$1" ]]; then
    echo "Usage: ./build-docker.sh <image_name>"
    echo "Example: ./build-docker.sh docker.io/coucou/biboumi"
    exit 1
fi

directory=$(dirname $0)
image_name=$1

echo $directory
docker build -t biboumi-base $directory -f $directory/Dockerfile.base
docker build -t $image_name  $directory -f $directory/Dockerfile --no-cache
