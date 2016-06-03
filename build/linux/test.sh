#!/usr/bin/env bash
set -xue

# FIXME
# Avahi daemon is available on Travis linux environment.
# Avahi is available on Travis machines, but doesn't seem to work (instances don't find each other).
#
# I know that is doesn't browse event localhost services if only loopback is available.
# But eth0 exists and is up !
# Checking settings and ip of eth0
ip link
ip addr
cat /etc/avahi/avahi-daemon.conf
cat /etc/nsswitch.conf

./localshare --version

if false; then
	# test transfer of dummy file
	head -c 100000 /dev/urandom > data
	./localshare -q -u data -p TEST &
	./localshare -v --yes -d -t test -n TEST
	wait
	cmp data test/data
fi

set +xue
