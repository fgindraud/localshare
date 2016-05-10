#ifndef TRANSFER_DOWNLOAD_H
#define TRANSFER_DOWNLOAD_H

#include <QAbstractSocket>
#include <QDataStream>

#include "style.h"
#include "settings.h"
#include "transfer.h"
#include "struct_item_model.h"

namespace Transfer {

class Download : public Item {
	Q_OBJECT

	/* Download object.
	 *
	 * Owned by their sockets, which are owned by the server.
	 */

private:
	enum Status {
		Error,

		WaitingHandshake,
		WaitingOffer,
		WaitingUserChoice,
		Transfering
	};

	Status status{Error};
	QString error{"init"};

	QAbstractSocket * socket;
	QDataStream socket_stream;
	qint64 next_message_size{-1};

	QString filename;
	qint64 size{-1};
	QString username;

public:
	Download (QAbstractSocket * connection)
	    : Item (connection), socket (connection), socket_stream (socket) {
		socket_stream.setVersion (Const::serializer_version);

		connect (socket, static_cast<void (QAbstractSocket::*) (QAbstractSocket::SocketError)> (
		                     &QAbstractSocket::error),
		         this, &Download::on_socket_error);
		connect (socket, &QAbstractSocket::readyRead, this, &Download::data_available);

		initiate_protocol ();
	}

private:
	// Model stuff
	void set_status (Status new_status) {
		status = new_status;
		emit data_changed (StatusField);
	}

	void set_filename (const QString & new_filename) {
		filename = new_filename;
		emit data_changed (FilenameField);
	}

	void set_username (const QString & new_username) {
		username = new_username;
		emit data_changed (PeerField);
	}

	void set_size (qint64 new_size) {
		size = new_size;
		emit data_changed (SizeField);
	}

	void failure (const QString & reason) {
		error = reason;
		set_status (Error);
		socket->abort ();
		socket_stream.resetStatus ();
		// TODO emit failed (reason);
	}

	QVariant data (int field, int role) const Q_DECL_OVERRIDE {
		switch (field) {
		case FilenameField: {
			// Display filename and transfer direction
			switch (role) {
			case Qt::DisplayRole:
				return filename;
			// TODO download path + editable ?
			case Qt::DecorationRole:
				return Icon::download ();
			}
		} break;
		case PeerField: {
			// Username (ip if not available), detailed to ip/port
			switch (role) {
			case Qt::DisplayRole:
				if (username.isEmpty ())
					return socket->peerAddress ().toString ();
				else
					return username;
				break;
			case Qt::StatusTipRole:
			case Qt::ToolTipRole:
				return tr ("%1 on port %2")
				    .arg (socket->peerAddress ().toString ())
				    .arg (socket->peerPort ());
			}
		} break;
		case SizeField: {
			// File size
			if (size >= 0) {
				switch (role) {
				case Qt::DisplayRole:
					return size_to_string (size);
				case Qt::StatusTipRole:
				case Qt::ToolTipRole:
					return tr ("%1B").arg (size);
				}
			}
		} break;
		case ProgressField: {
			// Transfer progress in %, and in bytes for tooltip
			switch (role) {
			case Qt::DisplayRole:
				return 1; // TODO
			case Qt::StatusTipRole:
			case Qt::ToolTipRole:
				return "1B/100B";
			}
		} break;
		case StatusField: {
			// Status message
			switch (role) {
			case Qt::DisplayRole: {
				switch (status) {
				case Error:
					return error;
				case WaitingHandshake:
					return tr ("Protocol checks");
				case WaitingOffer:
					return tr ("Waiting for file offer");
				case WaitingUserChoice:
					return {"choice ?"}; // TODO
				case Transfering:
					return tr ("Transfering");
				}
			} break;
			case Item::ButtonRole: {
				if (status == WaitingUserChoice)
					return QVariant::fromValue<Item::Buttons> (Item::AcceptButton | Item::CancelButton);
			} break;
			}
		} break;
		}
		return {};
	}

	QVariant compare_data (int field) const Q_DECL_OVERRIDE {
		switch (field) {
		case SizeField:
			return qint64 (size);
		case StatusField:
			return int(status);
		};
		return Item::compare_data (field);
	}

	void button_clicked (int field, Button btn) Q_DECL_OVERRIDE {
		// TODO
		qDebug () << "download: button_clicked" << field << btn;
	}

	/* Protocol implementation
	 */
private slots:
	void on_socket_error (QTcpSocket::SocketError) {
		failure (tr ("Network error: ") + socket->errorString ());
	}

	void data_available (void) {
		switch (status) {
		case Error:
			break;
		case WaitingHandshake: {
			if (!da_waiting_handshake ())
				break;
			set_status (WaitingOffer);
		}
		case WaitingOffer: {
			if (!da_waiting_offer ())
				break;
			if (Settings::DownloadAuto ().get ()) {
				set_status (Transfering);
			} else {
				set_status (WaitingUserChoice);
			}
		}
		case WaitingUserChoice:
			break;
		case Transfering: {
			if (!da_transfering ())
				break;
		}
		}
	}

private:
	void initiate_protocol (void) {
		set_status (WaitingHandshake);
		socket_stream << Const::protocol_magic << Const::protocol_version;
	}

	template <typename Msg> void send_message (const Msg & msg) {
		auto s = Sizes::get_serialized_size (Msg::code (), msg);
		Q_ASSERT (s <= Sizes::message_size_max);
		socket_stream << static_cast<Sizes::MessageSize> (s) << Msg::code () << msg;
	}

	bool check_datastream (void) {
		switch (socket_stream.status ()) {
		case QDataStream::Ok:
			return true;
		default:
			failure (tr ("Protocol data corrupted"));
			return false;
		}
	}

	bool da_waiting_handshake (void) {
		// Received handshake
		if (socket->bytesAvailable () < sizes.handshake)
			return false;
		std::remove_const<decltype (Const::protocol_magic)>::type magic;
		std::remove_const<decltype (Const::protocol_version)>::type version;
		socket_stream >> magic >> version;
		if (!check_datastream ())
			return false;
		if (magic != Const::protocol_magic) {
			failure (tr ("Protocol check failed"));
			return false;
		}
		if (version != Const::protocol_version) {
			failure (tr ("Wrong protocol version: peer=%1, ours=%2")
			             .arg (version)
			             .arg (Const::protocol_version));
			return false;
		}
		return true;
	}

	bool da_waiting_offer (void) {
		// Read message size
		if (next_message_size == -1) {
			if (socket->bytesAvailable () < sizes.message_size)
				return false;
			Sizes::MessageSize s;
			socket_stream >> s;
			if (!check_datastream ())
				return false;
			next_message_size = static_cast<qint64> (s);
		}
		// Read message code then message itself (all interruption are errors)
		if (socket->bytesAvailable () < next_message_size)
			return false;
		next_message_size = -1;
		Message::Code code;
		socket_stream >> code;
		if (!check_datastream ())
			return false;
		if (code != Message::Offer::code ()) {
			failure (tr ("Protocol error"));
			return false;
		}
		Message::Offer offer;
		socket_stream >> offer;
		if (!check_datastream ())
			return false;
		set_username (offer.username);
		set_filename (offer.filename);
		set_size (offer.size);
		return true;
	}

	bool da_transfering (void) { return false; }
};
}

#endif
