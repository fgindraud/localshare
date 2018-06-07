#!/usr/bin/env bash
set -xue

# Pack binary
strip localshare

cp localshare localshare-linux-$(uname -m)

set +xue
