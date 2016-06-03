#!/usr/bin/env bash
set -xue

EXEC="./localshare.app/Contents/MacOS/localshare"

# Bonjour is already available

$EXEC --version

# test transfer of dummy file
head -c 100000 /dev/urandom > data
$EXEC -q -u data -p TEST &
$EXEC -v --yes -d -t test -n TEST
wait
cmp data test/data

set +xue
