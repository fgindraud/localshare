#include "network.h"

/* ------ Zeroconf ----- */

ZeroconfHandler::ZeroconfHandler (Settings & settings) :
	mBrowser (), mService (),
	mName (settings.name ()),
	mPort (settings.tcpPort ())
{
	QObject::connect (&mBrowser, SIGNAL (serviceEntryAdded (QString)),
			this, SLOT (internalAddPeer (QString)));
	QObject::connect (&mBrowser, SIGNAL (serviceEntryRemoved (QString)),
			this, SLOT (internalRemovePeer (QString)));
}

void ZeroconfHandler::start (void) {
	// Service declaration
	mService.registerService (mName, mPort, AVAHI_SERVICE_NAME);

	// Start service browsing
	mBrowser.browse (AVAHI_SERVICE_NAME);
}

void ZeroconfHandler::settingsChanged (void) {
	qDebug () << "Not implemented";
}

void ZeroconfHandler::internalAddPeer (QString name) {
	ZConfServiceEntry entry = mBrowser.serviceEntry (name);

	// Add peer if valid and not us
	if (entry.isValid () && name != mName) {
		ZeroconfPeer newPeer (name, entry);
		emit addPeer (newPeer);
	}
}

void ZeroconfHandler::internalRemovePeer (QString name) {
	emit removePeer (name);
}

/* ------ Tcp server ----- */

TcpServer::TcpServer (Settings & settings) : QTcpServer () {
	if (not listen (QHostAddress::Any, settings.tcpPort ()))
		Message::error ("Tcp server",
				"Tcp server error : " + errorString ());

	// connect internal signal
}

