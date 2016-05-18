#!/usr/bin/env bash
set -xue

sudo apt-get -qq update
sudo apt-get install \
	qt5-default \
	qt5-qmake \
	libavahi-compat-libdnssd-dev

set +xue
