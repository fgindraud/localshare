#!/usr/bin/env bash
set -xue

# Get deps (Qt, Bonjour)
sudo apt-get -y -q update
sudo apt-get -y -q install \
	qt5-default \
	libqt5svg5-dev \
	qt5-qmake \
	libavahi-compat-libdnssd-dev

set +xue
