#ifndef TRANSFER_H
#define TRANSFER_H

#include <limits>
#include <QtNetwork>

#include "localshare.h"
#include "discovery.h"

namespace Transfer {

/*
 * Helpers to determine size of serialized data.
 */
class ByteCounterDevice : public QIODevice {
	// Dummy device that just counts the amount of bytes written to it
public:
	ByteCounterDevice (QObject * parent = nullptr) : QIODevice (parent) {
		open (QIODevice::WriteOnly);
	}

protected:
	qint64 writeData (const char *, qint64 max_size) {
		qDebug () << max_size;
		return max_size;
	}
	qint64 readData (char *, qint64) { return -1; }
};

inline void output_to_stream (QDataStream &) {}
template <typename Head, typename... T>
inline void output_to_stream (QDataStream & stream, const Head & h, const T &... args) {
	stream << h;
	output_to_stream (stream, args...);
}

template <typename... T> inline quint64 get_serialized_size (const T &... args) {
	ByteCounterDevice dev;
	QDataStream s{&dev};
	s.setVersion (Const::serializer_version);
	output_to_stream (s, args...);
	Q_ASSERT (dev.pos () > 0);
	return static_cast<quint64> (dev.pos ());
}

struct Payload {
	QString filename;
	quint64 size;
};

class Protocol {
public:
	using BlockSize = quint16;

	const quint64 block_size_size;
	const quint64 handshake_size;

	Protocol ()
	    : block_size_size (get_serialized_size (BlockSize ())),
	      handshake_size (get_serialized_size (Const::protocol_magic, Const::protocol_version)) {}
};

class ByteAvailableConstTransition : public QSignalTransition {
	Q_OBJECT
private:
	const QIODevice * device;
	quint64 min;

public:
	ByteAvailableConstTransition (const QIODevice * device, quint64 min, const QObject * sender,
	                              const char * signal)
	    : QSignalTransition (sender, signal), device (device), min (min) {}

	bool eventTest (QEvent * event) {
		if (!QSignalTransition::eventTest (event))
			return false;
		return static_cast<quint64> (device->bytesAvailable ()) >= min;
	}
};

class Upload : public QObject {
	Q_OBJECT

signals:
	void failed (QString);

private:
	QTcpSocket socket;
	QDataStream socket_stream;

public:
	Upload (const Discovery::Peer & peer, QObject * parent = nullptr)
	    : QObject (parent), socket_stream (&socket) {
		socket_stream.setVersion (Const::serializer_version);

		// Protocol state machine
		auto m = new QStateMachine (&socket);

		// Alive connection group
		auto alive = new QState (m);
		m->setInitialState (alive);

		auto connecting = new QState (alive);
		alive->setInitialState (connecting);
		connect (connecting, &QState::entered, [&, peer = peer ] {
			socket.connectToHost (peer.address, peer.port);
			qDebug () << "connecting to" << peer;
		});

		auto connected_waiting_handshake = new QState (alive);
		connect (connected_waiting_handshake, &QState::entered, [&] {
			socket_stream << Const::protocol_magic << Const::protocol_version;
			qDebug () << "sending handshake";
		});
		{ connecting->addTransition (&socket, &QTcpSocket::connected, connected_waiting_handshake); }

		auto handshake_received = new QState (alive);
		connect (handshake_received, &QState::entered, [] { qDebug () << "received handshake"; });
		{
			auto 
		}

		// Error state
		auto dead = new QState (m);
		alive->addTransition (
		    &socket, static_cast<void (QTcpSocket::*) (QTcpSocket::SocketError)> (&QTcpSocket::error),
		    dead);
		connect (dead, &QState::entered, [&] {
			emit failed (socket.errorString ());
			qDebug () << "error" << socket.errorString ();
			socket.abort ();
		});

		// End
		auto complete = new QFinalState (m);
		connect (complete, &QState::entered, [] { qDebug () << "complete"; });

		m->start ();
	}
};

#if 0
class Protocol : public QObject {
	Q_OBJECT

	/* Uploader         Downloader
	 * ---[open connection]--->
	 * ---[magic+ver]--->
	 * <---[magic+ver]---
	 * if magic/ver doesn't match => abort
	 * ---[offer]--->
	 * if accepted {
	 * <---[accepted]---
	 * ---[chunks]--->
	 * } else {
	 * <---[rejected]---
	 * }
	 * stop
	 */
public:
	enum class Status { Handshake };
	enum class Error { HandshakeMagic, HandshakeVersion };

private:
	using BlockSizeType = quint16;
	static const auto handshake_size =
	    get_serialized_size (Const::protocol_magic, Const::protocol_version);
	static const auto block_size_size = get_serialized_size (BlockSizeType ());

	QTcpSocket * socket;
	QDataStream stream;

	Status status;

	BlockSizeType following_block_size{0};

public:
	Protocol (QTcpSocket * socket) : QObject (socket) {
		connect (socket, &QTcpSocket::readyRead, this, &Protocol::has_pending_data);
	}

	template <typename Msg> void send_message (const Msg & msg) {}

private slots:
	void has_pending_data (void) {
		if (status == Status::WaitingHandshake) {

		} else {

			if (following_block_size == 0) {
			}
		}
	}
};

class Upload : public QObject {
	Q_OBJECT

private:
	enum Status { Connecting, WaitingHandshake };

	QTcpSocket * socket;
	QDataStream socket_stream;
	Status status;

	qint64 blah;

public:
	Upload (const Discovery::Peer & peer, const Payload & payload, QObject * parent = nullptr)
	    : QObject (parent),
	      socket (new QTcpSocket (this)),
	      socket_stream (socket),
	      status (Connecting) {
		socket_stream.setVersion (Const::serializer_version);

		connect (socket, &QTcpSocket::connected, this, &Upload::has_connected);
		connect (socket, &QTcpSocket::readyRead, this, &Upload::has_pending_data);

		socket->connectToHost (peer.address, peer.port);
	}

	template <typename Msg> void send_message (const Msg & msg) {
		auto msg_size = get_serialized_size (msg);
		Q_ASSERT (msg_size < std::numeric_limits<Protocol::BlockSizeType>::max ());
		socket_stream << static_cast<Protocol::BlockSizeType> (msg_size) << msg;
	}

private slots:
	void has_connected (void) {
		Q_ASSERT (status == Connecting);
		socket_stream << Const::protocol_magic << Const::protocol_version;
		status = WaitingHandshake;
	}

	void has_pending_data (void) {
		while (true) {
			switch (status) {
			case Connecting:
				qCritical () << "Received data in Connecting state";
				return;
			case WaitingHandshake: {
				if (socket->bytesAvailable () < 4) {
				}
				break;
			}
			}
		}
	}
};

#endif

class Server : public QObject {
	Q_OBJECT

private:
	QTcpServer server;

public:
	Server (QObject * parent = nullptr) : QObject (parent) {
		server.listen (); // any port
		connect (&server, &QTcpServer::newConnection, this, &Server::handle_new_connection);
	}
	quint16 port (void) const { return server.serverPort (); }

private slots:
	void handle_new_connection (void) {
		auto socket = server.nextPendingConnection ();
		qDebug () << "server:new" << socket->peerAddress ();
		socket->deleteLater ();
	}
};
}

#endif
