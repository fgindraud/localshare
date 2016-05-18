#!/usr/bin/env bash
set -xue

brew update
brew install qt5
export PATH="$(brew --prefix qt5)/bin:${PATH}"

set +xue
