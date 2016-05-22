#!/usr/bin/env bash
set -xue

# Pack binary
strip localshare
upx -9 localshare || true

cp localshare localshare-linux-$(uname -m)

set +xue
