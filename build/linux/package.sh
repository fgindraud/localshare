#!/usr/bin/env bash
set -xue

# Pack binary
strip localshare
upx -9 localshare

cp localshare localshare-linux-$(uname -m)

set +xue
