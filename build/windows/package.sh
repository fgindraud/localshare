#!/usr/bin/env bash
set -xue

# Reduce binary size
strip release/localshare.exe
upx -9 release/localshare.exe

cp release/localshare.exe localshare-windows32.exe

set +xue
