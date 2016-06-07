/* Localshare - Small file sharing application for the local network.
 * Copyright (C) 2016 Francois Gindraud
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef CORE_SERVER_H
#define CORE_SERVER_H

#include <QTcpServer>
#include <QtGlobal>

#include "compatibility.h"
#include "core_transfer.h"

namespace Transfer {

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
