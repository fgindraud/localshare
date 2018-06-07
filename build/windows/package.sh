#!/usr/bin/env bash
set -xue

# Reduce binary size
strip release/localshare.exe

cp release/localshare.exe localshare-windows-${BITS}.exe

set +xue
