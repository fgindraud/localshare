#!/usr/bin/env bash
set -xue

# Target is 32b
MXE_TARGET=i686-w64-mingw32.static
MXE_DIR=/usr/lib/mxe

### Get QT and binutils ###

# Add MXE ppa
echo "deb http://pkg.mxe.cc/repos/apt/debian wheezy main" \
	| sudo tee /etc/apt/sources.list.d/mxeapt.list
sudo apt-key adv --keyserver keyserver.ubuntu.com \
	--recv-keys D43A795B73B16ABE9643FE1AFD8FFF16DB45C6AB

# Install cross compiled Qt and binutils (and upx for binary compression)
sudo apt-get -y -q update
sudo apt-get -y install \
	upx-ucl \
	mxe-${MXE_TARGET}-qtbase \
	mxe-${MXE_TARGET}-binutils

# Redirect to right qmake / g++
export PATH="${MXE_DIR}/usr/bin:${PATH}"
function qmake () {
	"${MXE_TARGET}-qmake-qt5" "$@"
}

### Bonjour apple library ###

# Download mDNSResponder sources
MDNS="mDNSResponder-576.30.4"
MDNS_TARBALL="${MDNS}.tar.gz"
curl "https://opensource.apple.com/tarballs/mDNSResponder/${MDNS_TARBALL}" \
	-o "${MDNS_TARBALL}"
tar xf "${MDNS_TARBALL}"

# Take windows DLL Stub (DLL loader) from mDNSResponder sources
cp "${MDNS}/mDNSShared/dns_sd.h" "src/dns_sd.h"
cp "${MDNS}/mDNSWindows/DLLStub/DLLStub.h" "src/"
cp "${MDNS}/mDNSWindows/DLLStub/DLLStub.cpp" "src/"

# It will be compiled by qmake

set +xue
