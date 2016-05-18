LocalShare - Local file sharing application
===========================================

[![Build Status](https://travis-ci.org/lereldarion/qt-localshare.svg?branch=master)](https://travis-ci.org/lereldarion/qt-localshare)

Small graphical application to send a file to a local peer.
Peers on the local network are automatically discovered using Bonjour/Zeroconf.
Distant peer can be manually added (but performing a transfer requires an open firewall on the destination).

Setup
-----

Compilation:
```
qmake
make
```

Requires Qt >= 5.2, Bonjour support (see below) and c++11 compiler support.
Details about dependencies can be found in the `build/*/requirement.sh` files.

Binaries can be found in the release section.
They are standalone (icons & such are included), but they do not include Qt or Bonjour libraries.
Localshare may store some settings at user level (storage depends on the system, see the QtCore/QSettings documentation).

mDNS browsing and resolution
----------------------------

This application uses *Zeroconf* to find its peers on the local network.
More precisely, it uses the *Bonjour* API to implement mDNS Service Discovery.
*Zeroconf* support depends on the system:
* Linux:
	- Support through *Avahi* daemon and libraries
	- *Avahi* should be configured to resolve `.local` hostnames (https://wiki.archlinux.org/index.php/avahi#Hostname_resolution)
	- *Bonjour* API compatibility layer should be installed (`avahi-compat-libdns_sd`)
* Mac:
	- Bonjour is native to Mac, no library required
* Windows:
	- **Not tested**
	- Support through *mDNSResponder* (Bonjour Windows service, provided by apple)

Status
------

Done:
* mDNS browsing
* settings
* interface:
	* peer list:
		* automatically filled by discovery
		* supports manual peers (added by ip/port)
	* transfers: working (basic)
	* nice icons (credits to http://picol.org)
* transfer protocol:
	* file by file transfer

Todo:
* transfer protocol:
	* improve perf
	* directories
	* pre filter stuff in Transfer::Server before showing it
* discovery browser/service error handling (low probability)
* get attention if minimized (modified icon / OS specific way)

