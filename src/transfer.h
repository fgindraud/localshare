#ifndef TRANSFER_H
#define TRANSFER_H

#include <QDataStream>
#include <QTcpServer>
#include <QTcpSocket>
#include <QAbstractSocket>
#include <limits>
#include <type_traits>

#include "localshare.h"
#include "struct_item_model.h"

namespace Transfer {

class Server : public QObject {
	Q_OBJECT

	/* Server object.
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
			qDebug () << "server:new" << socket->peerAddress () << socket->peerPort ();
			emit new_connection (socket);
		});
	}
	quint16 port (void) const { return server.serverPort (); }
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
		static constexpr Code code (void) { return 0x42; }
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
		static constexpr Code code (void) { return 0x43; }
		// No data needed
	};
	inline QDataStream & operator<<(QDataStream & stream, const Accept &) { return stream; }
	inline QDataStream & operator>>(QDataStream & stream, Accept &) { return stream; }

	struct Reject {
		// Reject the file offer
		static constexpr Code code (void) { return 0x44; }
		// No data needed
	};
	inline QDataStream & operator<<(QDataStream & stream, const Reject &) { return stream; }
	inline QDataStream & operator>>(QDataStream & stream, Reject &) { return stream; }
}

class Sizes {
	/* Stores information on byte size of specific protocol elements.
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

extern Sizes sizes; // Global precomputed size info (defined in main.cpp)

class Model : public StructItemModel {
	Q_OBJECT

public:
	enum Field { Filename, Peer, Size, Status, Num }; // Items

	Model (QObject * parent = nullptr) : StructItemModel (Num, parent) {}

	QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const {
		if (!(role == Qt::DisplayRole && orientation == Qt::Horizontal))
			return {};
		switch (section) {
		case Filename:
			return tr ("Filename");
		case Peer:
			return tr ("Peer");
		case Size:
			return tr ("File size");
		case Status:
			return tr ("Status");
		default:
			return {};
		}
	}
};

inline QString size_to_string (qint64 size) {
	double num = size;
	double increment = 1024.0;
	static QString suffixes[] = {QObject::tr ("B"),
	                             QObject::tr ("KiB"),
	                             QObject::tr ("MiB"),
	                             QObject::tr ("GiB"),
	                             QObject::tr ("TiB"),
	                             QObject::tr ("PiB"),
	                             {}};
	int unit_idx = 0;
	while (num >= increment && !suffixes[unit_idx + 1].isEmpty ()) {
		unit_idx++;
		num /= increment;
	}
	return QString ().setNum (num, 'f', 2) + suffixes[unit_idx];
}
}

#endif
