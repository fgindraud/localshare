#!/usr/bin/env bash
#set -xue

# FIXME
# Avahi daemon is available on Travis linux environment.
# Avahi is available on Travis machines, but doesn't seem to work (instances don't find each other).
#
# I know that is doesn't browse event localhost services if only loopback is available.
# eth0 is up, has an address, and avahi/nsswitch config seems ok...
# Test with avahi tools...
ip link
ip addr
cat /etc/avahi/avahi-daemon.conf
cat /etc/nsswitch.conf

avahi-publish -s blah _localshare._tcp 42042 &
sleep 5s
avahi-browse -tva
killall avahi-publish

./localshare --version

# test transfer of dummy file
head -c 100000 /dev/urandom > data
./localshare -q -u data -p TEST &
./localshare -v --yes -d -t test -n TEST
wait
cmp data test/data

#set +xue
