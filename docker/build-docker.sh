#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/..

DOCKER_IMAGE=${DOCKER_IMAGE:-cspnpay/cspnd-develop}
DOCKER_TAG=${DOCKER_TAG:-latest}

BUILD_DIR=${BUILD_DIR:-.}

rm docker/bin/*
mkdir docker/bin
cp $BUILD_DIR/src/cspnd docker/bin/
cp $BUILD_DIR/src/cspn-cli docker/bin/
cp $BUILD_DIR/src/cspn-tx docker/bin/
strip docker/bin/cspnd
strip docker/bin/cspn-cli
strip docker/bin/cspn-tx

docker build --pull -t $DOCKER_IMAGE:$DOCKER_TAG -f docker/Dockerfile docker
