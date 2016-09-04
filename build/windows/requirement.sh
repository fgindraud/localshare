#!/usr/bin/env bash
set -xue

# Target is 32b
if [ "$BITS" = 32 ]; then
	MXE_TARGET=i686-w64-mingw32.static
elif [ "$BITS" = 64 ]; then
	MXE_TARGET=x86-64-w64-mingw32.static
else
	exit 1;
fi
MXE_EXEC="${MXE_TARGET/x86-64/x86_64}"
MXE_DIR=/usr/lib/mxe

### Get QT and binutils ###

# Add MXE ppa
echo "deb http://pkg.mxe.cc/repos/apt/debian wheezy main" \
	| sudo tee /etc/apt/sources.list.d/mxeapt.list
sudo apt-key adv --keyserver keyserver.ubuntu.com \
	--recv-keys D43A795B73B16ABE9643FE1AFD8FFF16DB45C6AB

# Install cross compiled Qt and binutils (and upx for binary compression)
sudo apt-get -y -q update
sudo apt-get -y -q install \
	upx-ucl \
	mxe-${MXE_TARGET}-qtbase \
	mxe-${MXE_TARGET}-qtsvg \
	mxe-${MXE_TARGET}-binutils

# Redirect to right qmake and g++
export PATH="${MXE_DIR}/usr/${MXE_TARGET}/qt5/bin:${PATH}"
export PATH="${MXE_DIR}/usr/bin:${PATH}"

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
