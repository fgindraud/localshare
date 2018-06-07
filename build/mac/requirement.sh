#!/usr/bin/env bash
set -xue

# Install qt5 from brew
brew install qt5

# Add path to find qmake (that will handle all other paths)
export PATH="$(brew --prefix qt5)/bin:${PATH}"

set +xue
