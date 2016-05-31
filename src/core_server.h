#pragma once
#ifndef CORE_SERVER_H
#define CORE_SERVER_H

#include <QTcpServer>
#include <QtGlobal>

#include "compatibility.h"
#include "core_transfer.h"

namespace Transfer {

// FIXME old object, will be deleted soon
class ServerOld : public QObject {
	Q_OBJECT
private:
	QTcpServer server;

signals:
	void new_connection (QAbstractSocket * socket);

public:
	ServerOld (QObject * parent = nullptr) : QObject (parent) {
		server.listen (); // any port
		connect (&server, &QTcpServer::newConnection, [this] {
			auto socket = server.nextPendingConnection ();
			emit new_connection (socket);
		});
	}
	quint16 port (void) const { return server.serverPort (); }
};

/* Receive and store incoming download request.
 * When they have all metadata ready, give them to the rest of the application.
 * A QObject should be tied to download_ready().
 * It should take ownership of the Download object.
 *
 * Any error in the server object is fatal to the application.
 */
class Server : public QObject {
	Q_OBJECT

	// TODO limit pending connections ?

private:
	QTcpServer server;

signals:
	void download_ready (Transfer::Download * download);

public:
	Server (QObject * parent = nullptr) : QObject (parent) {
		connect (&server, &QTcpServer::acceptError, this, &Server::server_error);
		if (!server.listen ()) {
			server_error ();
			return;
		}
		connect (&server, &QTcpServer::newConnection, [this] {
			while (server.hasPendingConnections ()) {
				auto socket = server.nextPendingConnection ();
				auto download = new Transfer::Download (socket, this);
				connect (download, &Transfer::Download::failed, this, &Server::download_failed);
				connect (download, &Transfer::Download::status_changed, this,
				         &Server::download_status_changed);
			}
		});
	}

	quint16 port (void) const { return server.serverPort (); }

private slots:
	void server_error (void) { qFatal ("Server failed: %s", qUtf8Printable (server.errorString ())); }

	void download_failed (void) {
		// Warn, and destroy it
		auto download = qobject_cast<Transfer::Download *> (sender ());
		Q_ASSERT (download);
		qWarning ("Server: Buffered Download failed: %s", qUtf8Printable (download->get_error ()));
		download->deleteLater ();
	}
	void download_status_changed (Transfer::Download::Status new_status) {
		if (new_status == Transfer::Download::WaitingForUserChoice) {
			// Give it to external structures
			auto download = qobject_cast<Transfer::Download *> (sender ());
			Q_ASSERT (download);
			disconnect (download, &Transfer::Download::failed, this, &Server::download_failed);
			disconnect (download, &Transfer::Download::status_changed, this,
			            &Server::download_status_changed);
			emit download_ready (download);
		}
	}
};
}

#endif
