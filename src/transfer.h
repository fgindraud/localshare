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

// This file contains a lot of utilities for transfers
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

namespace Message {
	/* Small classes that define protocol messages.
	 * Each message class is associated with a unique code.
	 * This code is given by static code() (FIXME move to constexpr value when clang stops failing).
	 * Each class is QDataStream capable.
	 *
	 * Protocol:
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
		QString filename; // Name of file
		qint64 size;
	};
	inline QDataStream & operator<<(QDataStream & stream, const Offer & req) {
		return stream << req.username << req.filename << req.size;
	}
	inline QDataStream & operator>>(QDataStream & stream, Offer & req) {
		return stream >> req.username >> req.filename >> req.size;
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

class Item : public StructItem {
	Q_OBJECT

	/* Base transfer item class.
	 * Subclassed by upload or download.
	 */
public:
	// Fields that are supported
	enum Field { FilenameField, PeerField, SizeField, ProgressField, StatusField, NbFields };

	/* Status can have buttons to interact with user (accept/abort transfer).
	 * The View/Model system doesn't support buttons very easily.
	 * The chosen approach is to have a new Role to tell which buttons are enabled.
	 * This Role stores an OR of flags, and an invalid QVariant is treated as a 0 flag.
	 * The buttons are manually painted on the view by a custom delegate.
	 * The delegate also catches click events.
	 * TODO click event ?
	 */
	enum Role { ButtonRole = Qt::UserRole };
	enum Button { NoButton = 0x0, AcceptButton = 0x1, CancelButton = 0x2, ChangeDownloadPathButton = 0x4 };
	Q_DECLARE_FLAGS (Buttons, Button);
	Q_FLAG (Buttons);

public:
	Item (QObject * parent = nullptr) : StructItem (NbFields, parent) {}

	virtual void button_clicked (int field, Button btn) = 0;
};
Q_DECLARE_OPERATORS_FOR_FLAGS (Item::Buttons);

class Model : public StructItemModel {
	Q_OBJECT

	/* Transfer list model.
	 * Adds headers and dispatch button_clicked.
	 */
public:
	Model (QObject * parent = nullptr) : StructItemModel (Item::NbFields, parent) {}

	QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const {
		if (!(role == Qt::DisplayRole && orientation == Qt::Horizontal))
			return {};
		switch (section) {
		case Item::FilenameField:
			return tr ("File");
		case Item::PeerField:
			return tr ("Peer");
		case Item::SizeField:
			return tr ("File size");
		case Item::ProgressField:
			return tr ("Transferred");
		case Item::StatusField:
			return tr ("Status");
		default:
			return {};
		}
	}

public slots:
	void button_clicked (const QModelIndex & index, Item::Button btn) {
		if (has_item (index))
			return get_item_t<Item *> (index)->button_clicked (index.column (), btn);
		qCritical () << "button_clicked on invalid item" << index << btn;
	}
};

inline QString size_to_string (qint64 size) {
	// Find correct unit to fit size.
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
