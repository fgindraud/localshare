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
// Check that size of received data is ok
// start messages by magic, prot_ver, packet size

struct Payload {
	QString filename;
	quint64 size;
};

class Protocol {
public:
	using BlockSizeType = quint16;
	const quint64 block_size_size;
	const quint64 handshake_size;

	Protocol ()
	    : block_size_size (get_serialized_size (BlockSizeType ())),
	      handshake_size (get_serialized_size (Const::protocol_magic, Const::protocol_version)) {}
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
#endif

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

#if 0
class Download : public QObject {
	Q_OBJECT

private:
	QTcpSocket * socket; // Also our parent
	QDataStream tcp_stream;

public:
	Download (QTcpSocket * socket_) : QObject (socket_), socket (socket_), tcp_stream (socket_) {
		tcp_stream.setVersion (Const::serializer_version);

		connect (socket, &QTcpSocket::readyRead, this, &Download::has_pending_data);
	}

	template <typename Msg> void send_message (const Msg & msg) {
		QByteArray block;
		QDataStream stream{&block};
		stream.setVersion (Const::serializer_version);
		stream << msg;

		Q_ASSERT (static_cast<std::size_t> (block.size ()) < std::numeric_limits<quint16>::max ());
		tcp_stream << Const::protocol_magic << Const::protocol_version
		           << static_cast<quint16> (block.size ());
		tcp_stream.writeRawData (block.constData (), block.size ());
	}

private slots:
	void has_pending_data (void) {
		// blah
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
		delete socket;
	}
};
}

#endif
