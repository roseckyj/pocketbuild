#!/bin/sh

set -e

mkdir -p /tmp/build
cd /tmp/build
cmake /project/src
make
mkdir -p /project/build
cp ./build.app /project/build/