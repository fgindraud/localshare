#!/usr/bin/env bash
set -xue

sudo apt-get -y -q update
sudo apt-get -y install \
	qt5-default \
	qt5-qmake \
	libavahi-compat-libdnssd-dev

set +xue
