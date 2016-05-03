#ifndef TRANSFER_H
#define TRANSFER_H

#include <QtNetwork>
#include <limits>
#include <type_traits>

#include "localshare.h"
#include "transfer_model.h"

namespace Transfer {

class Upload : public Item {
	Q_OBJECT

	/* Upload objects are owned by the model.
	 */

private:
	const Peer peer;
	const QString filename;

public:
	Upload (const Peer & peer, const QString & filename, QObject * parent = nullptr)
	    : Item (Direction::Upload, parent), peer (peer), filename (filename) {}

	QString get_filename (void) const { return filename; }
	Peer get_peer (void) const { return peer; }
};

class Download : public Item {
	Q_OBJECT

	/* Download objects are owned by the socket, which are owned by the server.
	 */

private:
	QString filename{"<unknown>"};
	QString username;
	QAbstractSocket * socket;

public:
	Download (QAbstractSocket * connection)
	    : Item (Direction::Download, connection), socket (connection) {}

	QString get_filename (void) const { return filename; }
	Peer get_peer (void) const {
		return {username, QString (), socket->peerAddress (), socket->peerPort ()};
	}
};

#if 0
struct Payload {
	QString filename;
	qint64 size;
};

namespace Message {
	/* Protocol:
	 *
	 * Uploader         Downloader
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

	using Code = quint16;

	struct Offer {
		// Request for sending a file by the Uploader
		static constexpr Code code = 0x42;
		QString username; // Sender
		QString basename; // Name of file
		qint64 size;
	};
	inline QDataStream & operator<<(QDataStream & stream, const Offer & req) {
		return stream << req.username << req.basename << req.size;
	}
	inline QDataStream & operator>>(QDataStream & stream, Offer & req) {
		return stream >> req.username >> req.basename >> req.size;
	}

	struct Accept {
		// Accept the file offer
		static constexpr Code code = 0x43;
		// No data needed
	};
	inline QDataStream & operator<<(QDataStream & stream, const Accept &) { return stream; }
	inline QDataStream & operator>>(QDataStream & stream, Accept &) { return stream; }

	struct Reject {
		// Reject the file offer
		static constexpr Code code = 0x44;
		// No data needed
	};
	inline QDataStream & operator<<(QDataStream & stream, const Reject &) { return stream; }
	inline QDataStream & operator>>(QDataStream & stream, Reject &) { return stream; }
};

class Sizes {
	/* Stores information on byte suze of specific protocol elements.
	 */
private:
	// Device that just counts the amount of bytes written to it
	class DummyDevice : public QIODevice {
	public:
		DummyDevice (QObject * parent = nullptr) : QIODevice (parent) { open (QIODevice::WriteOnly); }

	protected:
		qint64 writeData (const char *, qint64 max_size) { return max_size; }
		qint64 readData (char *, qint64) { return -1; }
	};

	// Apply stream operator to list of args
	static inline void output_to_stream (QDataStream &) {}
	template <typename Head, typename... T>
	static inline void output_to_stream (QDataStream & stream, const Head & h, const T &... args) {
		stream << h;
		output_to_stream (stream, args...);
	}

public:
	// Get the size of args if they were serialized
	template <typename... T> static inline qint64 get_serialized_size (const T &... args) {
		DummyDevice dev;
		QDataStream s{&dev};
		s.setVersion (Const::serializer_version);
		output_to_stream (s, args...);
		Q_ASSERT (dev.pos () > 0);
		return dev.pos ();
	}

	/* Messages can vary in size and must be prefixed by their size.
	 * MessageSize is the type used to store that size.
	 * It can store up to message_size_max.
	 */
	using MessageSize = quint16;
	static constexpr auto message_size_max =
	    static_cast<qint64> (std::numeric_limits<MessageSize>::max ());

	/* Precomputed constants for serialized element byte size.
	 * Used to check for data availability before reading the QDataStream.
	 * Exposed as public as they are constant.
	 */
	const qint64 message_size; // Size of message_size data
	const qint64 handshake;    // Size of handshake

	Sizes ()
	    : message_size (get_serialized_size (MessageSize ())),
	      handshake (get_serialized_size (Const::protocol_magic, Const::protocol_version)) {}
};

class Upload : public QObject {
	Q_OBJECT

signals:
	void failed (QString);

private:
	enum class Status {
		Disconnected,

		Connecting,
		WaitingHandshake,
		WaitingRequestAnswer,
		Transfering
	};

	QTcpSocket socket;
	QDataStream socket_stream;

	Status status{Status::Disconnected};
	qint64 next_message_size{-1};

	Peer peer;
	Payload payload;
	Sizes sizes;

public:
	Upload (const Peer & peer, QObject * parent = nullptr)
	    : QObject (parent), socket_stream (&socket), peer (peer) {
		socket_stream.setVersion (Const::serializer_version);

		connect (&socket,
		         static_cast<void (QTcpSocket::*) (QTcpSocket::SocketError)> (&QTcpSocket::error), this,
		         &Upload::on_error);
		connect (&socket, &QTcpSocket::connected, this, &Upload::on_connected);
		connect (&socket, &QTcpSocket::readyRead, this, &Upload::data_available);
	}

private:
	void start (void) {
		Q_ASSERT (status == Status::Disconnected);
		status = Status::Connecting;
		socket.connectToHost (peer.address, peer.port);
	}

	void failure (const QString & reason) {
		status = Status::Disconnected;
		socket.abort ();
		socket_stream.resetStatus ();
		qDebug () << "Upload error:" << reason;
		emit failed (reason);
	}

	template <typename Msg> void send_message (const Msg & msg) {
		auto s = Sizes::get_serialized_size (Msg::code, msg);
		Q_ASSERT (s <= Sizes::message_size_max);
		socket_stream << static_cast<Sizes::MessageSize> (s) << Msg::code << msg;
	}

private slots:
	void on_error (QTcpSocket::SocketError) { failure (socket.errorString ()); }

	void on_connected (void) {
		Q_ASSERT (status == Status::Connecting);
		status = Status::WaitingHandshake;
		socket_stream << Const::protocol_magic << Const::protocol_version;
	}

	void data_available (void) {
		switch (status) {
		case Status::Disconnected:
		case Status::Connecting:
			qFatal ("received data while disconnected");
			break;
		case Status::WaitingHandshake: {
			if (!da_waiting_handshake ())
				break;
			status = Status::WaitingRequestAnswer;
		}
		case Status::WaitingRequestAnswer: {
			if (!da_waiting_request_answer ())
				break;
			status = Status::Transfering;
		}
		case Status::Transfering: {
			if (!da_transfering ())
				break;
		}
		}
	}

private:
	bool check_datastream (void) {
		switch (socket_stream.status ()) {
		case QDataStream::Ok:
			return true;
		default:
			failure ("Received corrupt data");
			return false;
		}
	}

	bool da_waiting_handshake (void) {
		// Received handshake
		if (socket.bytesAvailable () < sizes.handshake)
			return false;
		std::remove_const<decltype (Const::protocol_magic)>::type magic;
		std::remove_const<decltype (Const::protocol_version)>::type version;
		socket_stream >> magic >> version;
		if (!check_datastream ())
			return false;
		if (magic != Const::protocol_magic) {
			failure ("Wrong protocol");
			return false;
		}
		if (version != Const::protocol_version) {
			failure (QString ("Wrong protocol version: peer=%1, ours=%2")
			             .arg (version)
			             .arg (Const::protocol_version));
			return false;
		}
		// Send upload request
		send_message (Message::Offer{"username", "blah", 349}); // TODO
		return true;
	}

	bool da_waiting_request_answer (void) {
		// Read message size
		if (next_message_size == -1) {
			if (socket.bytesAvailable () < sizes.message_size)
				return false;
			Sizes::MessageSize s;
			socket_stream >> s;
			if (!check_datastream ())
				return false;
			next_message_size = static_cast<qint64> (s);
		}
		// Read message code then message itself (all interruption are errors)
		if (socket.bytesAvailable () < next_message_size)
			return false;
		next_message_size = -1;
		Message::Code code;
		socket_stream >> code;
		if (!check_datastream ())
			return false;
		switch (code) {
		case Message::Accept::code:
			// No data to read
			return true;
		case Message::Reject::code:
			// No data to read
			failure ("Transfer rejected by user");
			return false;
		default:
			failure ("Protocol error");
			return false;
		}
		return true;
	}

	bool da_transfering (void) { return false; }
};
#endif

class Server : public QObject {
	Q_OBJECT

private:
	QTcpServer server;

signals:
	void new_connection (QAbstractSocket * socket);

public:
	Server (QObject * parent = nullptr) : QObject (parent) {
		server.listen (); // any port
		connect (&server, &QTcpServer::newConnection, [this] {
			auto socket = server.nextPendingConnection ();
			qDebug () << "server:new" << socket->peerAddress () << socket->peerPort ();
			emit new_connection (socket);
		});
	}
	quint16 port (void) const { return server.serverPort (); }
};
}

#endif
