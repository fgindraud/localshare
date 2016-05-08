#ifndef TRANSFER_UPLOAD_H
#define TRANSFER_UPLOAD_H

#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QDataStream>

#include "style.h"
#include "transfer.h"
#include "struct_item_model.h"

namespace Transfer {

class Upload : public StructItem {
	Q_OBJECT

	/* Upload object.
	 *
	 * Usually owned by the model.
	 */

private:
	enum Status {
		Error,

		Connecting,
		WaitingHandshake,
		WaitingRequestAnswer,
		Transfering
	};

	Status status{Error};
	QString error{"init"};

	const Peer peer;
	const QString & our_username;
	QTcpSocket socket;
	QDataStream socket_stream;
	qint64 next_message_size{-1};

	QString filename;
	QFile file;

public:
	Upload (const Peer & peer, const QString & filepath, const QString & our_username,
	        QObject * parent = nullptr)
	    : StructItem (Model::Num, parent),
	      peer (peer),
	      our_username (our_username),
	      filename (QFileInfo (filepath).fileName ()),
	      file (filepath) {
		socket_stream.setVersion (Const::serializer_version);

		// Socket
		connect (&socket,
		         static_cast<void (QTcpSocket::*) (QTcpSocket::SocketError)> (&QTcpSocket::error), this,
		         &Upload::on_socket_error);
		connect (&socket, &QTcpSocket::connected, this, &Upload::on_connected);
		connect (&socket, &QTcpSocket::readyRead, this, &Upload::data_available);

		initiate_connection ();
	}

	/* Model funcs
	 */
private:
	void set_status (Status new_status) {
		status = new_status;
		emit data_changed (Model::Status);
	}

	void failure (const QString & reason) {
		error = reason;
		set_status (Error);
		socket.abort ();
		socket_stream.resetStatus ();
		// TODO emit failed (reason);
	}

	QVariant data (int elem, int role) const Q_DECL_OVERRIDE {
		switch (elem) {
		case Model::Filename: {
			// Filename indicates direction of transfer too
			switch (role) {
			case Qt::DisplayRole:
				return filename;
			case Qt::StatusTipRole:
			case Qt::ToolTipRole:
				return tr ("Uploading %1").arg (file.fileName ());
			case Qt::DecorationRole:
				return Icon::upload ();
			}
		} break;
		case Model::Peer: {
			// Username, detailed to hostname/ip/port
			switch (role) {
			case Qt::DisplayRole:
				return peer.username;
			case Qt::StatusTipRole:
			case Qt::ToolTipRole:
				return tr ("%1 (%2) on port %3")
				    .arg (peer.hostname)
				    .arg (peer.address.toString ())
				    .arg (peer.port);
			}
		} break;
		case Model::Size: {
			// File size
			switch (role) {
			case Qt::DisplayRole:
				return size_to_string (file.size ());
			case Qt::StatusTipRole:
			case Qt::ToolTipRole:
				return tr ("%1B").arg (file.size ());
			}
		} break;
		case Model::Progress: {
			// Transfer progress in %, and in bytes for tooltip
			switch (role) {
			case Qt::DisplayRole:
				return 42; // TODO
			case Qt::StatusTipRole:
			case Qt::ToolTipRole:
				return "42B/100B";
			}
		} break;
		case Model::Status: {
			// Status message
			if (role == Qt::DisplayRole) {
				switch (status) {
				case Error:
					return error;
				case Connecting:
					return tr ("Connecting");
				case WaitingHandshake:
					return tr ("Protocol checks");
				case WaitingRequestAnswer:
					return tr ("Waiting peer answer");
				case Transfering:
					return tr ("Transfering");
				}
			}
		} break;
		}
		return {};
	}

	QVariant compare_data (int elem) const Q_DECL_OVERRIDE {
		switch (elem) {
		case Model::Size:
			return qint64 (file.size ());
		case Model::Status:
			return int(status);
		};
		return StructItem::compare_data (elem);
	}

	/* Protocol implementation
	 */
private slots:
	void on_socket_error (QTcpSocket::SocketError) {
		failure (tr ("Network error: ") + socket.errorString ());
	}

	void on_connected (void) {
		Q_ASSERT (status == Connecting);
		set_status (WaitingHandshake);
		socket_stream.setDevice (&socket);
		socket_stream << Const::protocol_magic << Const::protocol_version;
	}

	void data_available (void) {
		switch (status) {
		case Error:
		case Connecting:
			break;
		case WaitingHandshake: {
			if (!da_waiting_handshake ())
				break;
			set_status (WaitingRequestAnswer);
		}
		case WaitingRequestAnswer: {
			if (!da_waiting_request_answer ())
				break;
			set_status (Transfering);
		}
		case Transfering: {
			if (!da_transfering ())
				break;
		}
		}
	}

private:
	void initiate_connection (void) {
		set_status (Connecting);
		socket.connectToHost (peer.address, peer.port);
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
		if (socket.bytesAvailable () < sizes.handshake)
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
		// Send upload request
		send_message (Message::Offer{our_username, filename, file.size ()});
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
		case Message::Accept::code ():
			// No data to read
			return true;
		case Message::Reject::code ():
			// No data to read
			failure (tr ("Transfer rejected by peer"));
			return false;
		default:
			failure (tr ("Protocol error"));
			return false;
		}
		return true;
	}

	bool da_transfering (void) { return false; }
};
}

#endif
