#!/usr/bin/env bash
set -xue

# Reduce binary size
strip release/localshare.exe
upx -9 release/localshare.exe || true

cp release/localshare.exe localshare-windows-${BITS}.exe

set +xue
