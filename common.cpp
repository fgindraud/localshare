#include "localshare.h"
#include "common.h"

#include <QMessageBox>

/* ----- Settings ----- */

Settings::Settings () : QSettings (APP_NAME, APP_NAME) {
}

/* network */
QString Settings::name (void) const {
	QString tmp = value ("network/name", "Unknown").toString ();
	tmp.truncate (NAME_SIZE_LIMIT);
	return tmp;
}

void Settings::setName (const QString & name) {
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

void Settings::setDownloadPath (const QString & path) {
	setValue ("download/path", path);
}

bool Settings::alwaysDownload (void) const {
	return value ("download/alwaysAccept", false).toBool ();
}

void Settings::setAlwaysDownload (bool always) {
	setValue ("download/alwaysAccept", always);
}

/* ----- Peer ----- */
Peer::Peer (const QString & name, const QHostAddress & address) : m_name (name), m_address (address) {
}

QString Peer::name (void) const {
	return m_name;
}

const QHostAddress & Peer::address (void) const {
	return m_address;
}

bool operator== (const Peer & a, const Peer & b) {
	return a.name () == b.name () && a.address () == b.address ();
}

/* ----- Message ----- */
void Message::error (const QString & title, const QString & message) {
	QMessageBox::critical (0, title, message);
	qApp->quit ();
}

void Message::warning (const QString & title, const QString & message) {
	QMessageBox::warning (0, title, message);
}

