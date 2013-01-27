#include "network.h"

/* ------ Zeroconf ----- */

ZeroconfHandler::ZeroconfHandler (Settings & settings) :
	m_browser (), m_service (),
	m_name (settings.name ()),
	m_port (settings.tcpPort ())
{
}

void ZeroconfHandler::start (void) {
	// Service declaration
	m_service.registerService (m_name, m_port, AVAHI_SERVICE_NAME);

	// Start service browsing
	m_browser.browse (AVAHI_SERVICE_NAME);
}

void ZeroconfHandler::settingsChanged (void) {
	qDebug () << "Not implemented";
}

void ZeroconfHandler::internalAddPeer (QString name) {
	ZConfServiceEntry entry = m_browser.serviceEntry (name);
	if (entry.isValid ())
		emit addPeer (ZeroconfPeer (entry));
}

void ZeroconfHandler::internalRemovePeer (QString name) {
	emit removePeer (name);
}



