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
They are mostly standalone:
- Icons are included for each platform.
- Linux: uses the local Qt and Avahi libraries.
- Mac OSX: assumes a homebrew install of Qt5 (Bonjour support is native).
- Windows: shipped with a statically linked Qt5, requires the mDNSResponder on the system.

Localshare may store some settings at user level (storage depends on the system, see the QtCore/QSettings documentation).

mDNS browsing and resolution
----------------------------

This application uses *Zeroconf* to find its peers on the local network.
More precisely, it uses the *Bonjour* API to implement mDNS Service Discovery.
*Zeroconf* support depends on the system:
* Linux:
	- Support through *Avahi* daemon and libraries.
	- *Bonjour* API compatibility layer must be installed (`avahi-compat-libdns_sd`).
	- The *Avahi* daemon must be running for discovery to work.
	- *Avahi* should be configured to resolve `.local` hostnames (https://wiki.archlinux.org/index.php/avahi#Hostname_resolution).
* Mac:
	- Bonjour is natively supported; no specific setup required.
* Windows:
	- Support through Bonjour Windows Service (*mDNSResponder*, provided by Apple).
	- mDNSResponder API library is statically compiled (see `build/windows/requirement.sh` and `localshare.pro`).
	- The mDNSResponder.exe service must be running for discovery to work.
	- I do not want to go through the complex Apple « [Windows Bundling Agreement](https://developer.apple.com/softwarelicensing/agreements/bonjour.php) », so I cannot distribute an installer of mDNSResponder. Some popular applications will install a copy of mDNSResponder (iTunes, Skype), so you might not need to install anything in this case (check for a "mDNSResponder.exe" service). If it is not the case, you can always try to find an installer on the web, or get it by registering as an Apple Developer and downloading the Bonjour SDK.

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

