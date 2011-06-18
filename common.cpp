#include "localshare.h"
#include "common.h"

Settings::Settings () : QSettings (APP_NAME, APP_NAME) {
}

/* network */
QString Settings::name (void) const {
	return value ("network/name", "Unknown").toString ();
}

void Settings::setName (QString & name) {
	setValue ("network/name", name);
}

quint16 Settings::udpPort (void) const {
	return static_cast<quint16> (value ("network/udpPort", DEFAULT_UDP_PORT).toUInt ());
}

void Settings::setUdpPort (quint16 port) {
	setValue ("network/udpPort", static_cast<uint> (port));
}

quint16 Settings::tcpPort (void) const {
	return static_cast<quint16> (value ("network/tcpPort", DEFAULT_TCP_PORT).toUInt ());
}

void Settings::setTcpPort (quint16 port) {
	setValue ("network/tcpPort", static_cast<uint> (port));
}

/* download */
QString Settings::downloadPath (void) const {
	return value ("download/path", QDir::homePath ()).toString ();
}

void Settings::setDownloadPath (QString & path) {
	setValue ("download/path", path);
}

bool Settings::alwaysDownload (void) const {
	return value ("download/alwaysAccept", false).toBool ();
}

void Settings::setAlwaysDownload (bool always) {
	setValue ("download/alwaysAccept", always);
}

