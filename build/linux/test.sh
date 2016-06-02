#!/usr/bin/env bash
set -xue

./localshare --version
# TODO test
pgrep avahi-daemon

set +xue
