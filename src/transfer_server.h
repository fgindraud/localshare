#ifndef TRANSFER_SERVER_H
#define TRANSFER_SERVER_H

#include <QDataStream>
#include <QTcpServer>
#include <QTcpSocket>

namespace Transfer {

class Server : public QObject {
	Q_OBJECT

	/* Server object.
	 * Nothing fancy, just a small wrapper around a QTcpServer
	 */
private:
	QTcpServer server;

signals:
	void new_connection (QAbstractSocket * socket);

public:
	Server (QObject * parent = nullptr) : QObject (parent) {
		server.listen (); // any port
		connect (&server, &QTcpServer::newConnection, [this] {
			auto socket = server.nextPendingConnection ();
			emit new_connection (socket);
		});
	}
	quint16 port (void) const { return server.serverPort (); }
};
}

#endif
