#!/usr/bin/env bash
set -xue

./localshare --version

# test transfer of dummy file
if pgrep avahi-daemon; then
	head -c 100000 /dev/urandom > data
	./localshare -q -u data -p TEST &
	./localshare -v --yes -d -t test -n TEST
	wait
	cmp data test/data
fi

set +xue
