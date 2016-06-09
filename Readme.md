Localshare - Local file sharing application
===========================================

[![Build Status](https://travis-ci.org/lereldarion/qt-localshare.svg?branch=master)](https://travis-ci.org/lereldarion/qt-localshare)
[![Localshare License](https://img.shields.io/badge/license-GPL3-blue.svg)](#license)
[![Latest release](https://img.shields.io/github/release/lereldarion/qt-localshare.svg)](https://github.com/lereldarion/qt-localshare/releases/latest)

Small graphical application to send a file to a local peer.
Peers on the local network are automatically discovered using Zeroconf mDNS service discovery.
Distant peers can be manually added by filling ip and port (but performing a transfer requires an open firewall on the destination).

A command line interface is also availablei (see `localshare --help`), and should work without requiring any graphical stack support.
The project can also be compiled with only the command line interface (see `localshare.pro`), reducing dependencies and executable size.

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
- Linux: uses the local Qt5 and Avahi Zeroconf libraries.
- Mac OSX: assumes a homebrew install of Qt5 (Bonjour support is native).
- Windows: shipped with a statically linked Qt5, requires a working mDNSResponder install.

Localshare may store some settings at user level (storage depends on the system, see the QtCore/QSettings documentation).

Zeroconf mDNS support
---------------------

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
* local network discovery
	* show errors to user
	* ability to restart on failure and change username
* settings
* gui interface:
	* peer list:
		* automatically filled by discovery
		* supports manual peers (added by ip/port)
	* transfers:
		* can manage multiple transfers
* console interface:
	* single upload/download
	* list peers
* transfer protocol:
	* used by both cli and gui
	* use chunks and file mapping for perf
	* can send directories or simple files
	* transfers are only shown when enough details has been gathered (file list)

Todo:
* Ip resolving (gui, mostly):
	* caching is bad (hostname ip can change...):
		* need to support both manual (by ip) and auto (by hostname) peers at the same time.
		* auto: ip is just indication, and is recomputed at every transfer ?
* CLI:
	* piping files ?
* protocol / gui / cli: support restarts ?
* directories:
	* better file listing for large dirs (step in gui)
	* see content before sending (uncheck stuff to not send it)
	* see content before downloading (and uncheck stuff too ?)
* get attention if minimized (modified icon / OS specific way)
* distant peers:
	* upnp firewall opening for distant peers ? (dangerous)
	* hardening of protocol ? (local link is easier to trust)
	* use authentification with ssl ? (QSslSocket)

String status (if you want to translate):
* All messages that the user can read use tr().
* Messages that go to qWarning/qDebug are untranslated.
* Internal strings:
	* Left raw if used in a context that doesn't need a QString
	* Small formats strings: use QStringLiteral
	* Left raw if perf is not needed (one time use)

License
-------

```
Localshare - Small file sharing application for the local network.
Copyright (C) 2016 Francois Gindraud

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
```

Icons from [picol](http://picol.org/) under the [Creative Commons-License BY](http://creativecommons.org/licenses/by/3.0/).
