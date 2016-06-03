#!/usr/bin/env bash
set -xue

# Avahi daemon is available on Travis linux environment.
# Avahi doesn't show services if there is only loopback.
# But on travis we have an eth0 link up and with Ip, so it should be ok.
# Avahi and  dns resolution of .local seem correctly configured

./localshare --version

# test transfer of dummy file
head -c 100000 /dev/urandom > data
./localshare -q -u data -p TEST &
./localshare -v --yes -d -t test -n TEST
wait
cmp data test/data

set +xue
