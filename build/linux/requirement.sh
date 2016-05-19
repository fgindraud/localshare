#!/usr/bin/env bash
set -xue

# Get deps (Qt, Bonjour, upx for binary packing)
sudo apt-get -y -q update
sudo apt-get -y install \
	upx-ucl \
	qt5-default \
	qt5-qmake \
	libavahi-compat-libdnssd-dev

set +xue
