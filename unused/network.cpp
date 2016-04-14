#include "network.h"

/* ------ Zeroconf ----- */

ZeroconfHandler::ZeroconfHandler () : mBrowser (), mService () {
	QObject::connect (&mBrowser, SIGNAL (serviceEntryAdded (QString)),
			this, SLOT (internalAddPeer (QString)));
	QObject::connect (&mBrowser, SIGNAL (serviceEntryRemoved (QString)),
			this, SLOT (internalRemovePeer (QString)));
}

void ZeroconfHandler::start (void) {
	// Service declaration
	mService.registerService (Settings::name (), Settings::tcpPort (), AVAHI_SERVICE_NAME);

	// Start service browsing
	mBrowser.browse (AVAHI_SERVICE_NAME);
}

void ZeroconfHandler::internalAddPeer (QString name) {
	ZConfServiceEntry entry = mBrowser.serviceEntry (name);

	// Add peer if valid and not us
	if (entry.isValid () && name != Settings::name ()) {
		ZeroconfPeer newPeer (name, entry);
		emit addPeer (newPeer);
	}
}

void ZeroconfHandler::internalRemovePeer (QString name) {
	emit removePeer (name);
}

/* ------ Tcp server ----- */

TcpServer::TcpServer () : QTcpServer () {
	// Start server
	if (not listen (QHostAddress::Any, Settings::tcpPort ()))
		Message::error ("Tcp server",
				"Tcp server error : " + errorString ());

	// Connect internal callback
	QObject::connect (this, SIGNAL (newConnection ()),
			this, SLOT (handleNewConnectionInternal ()));
}

TcpServer::~TcpServer () {
	// Kill pending connections
	foreach (InTransferHandler * p, mWaitingForHeaderConnections) {
		delete p;
	}
}

void TcpServer::handleNewConnectionInternal (void) {
	QTcpSocket * tmp;
	while ((tmp = nextPendingConnection ()) != 0)
		mWaitingForHeaderConnections.append (new InTransferHandler (tmp));
}

/* ------ Connection handler ------ */

InTransferHandler::InTransferHandler (QTcpSocket * socket) : mSocket (socket) {
	// Add internal handler binding
	QObject::connect (mSocket, SIGNAL (readyRead ()),
			this, SLOT (processData ()));

	// Init booleans
	mReceivedPeerName = false;
	mReceivedFileName = false;
	mReceivedFileSize = false;
}

InTransferHandler::~InTransferHandler () {
	delete mSocket;
}

void InTransferHandler::processData (void) {
}

