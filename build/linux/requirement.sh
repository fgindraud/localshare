#!/bin/bash
set -xue

sudo apt-get -qq update
sudo apt-get --yes install \
	qt5-default \
	qt5-qmake \
	libavahi-compat-libdnssd-dev

set +xue
