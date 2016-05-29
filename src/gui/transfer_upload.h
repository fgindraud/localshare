#pragma once
#ifndef GUI_TRANSFER_UPLOAD_H
#define GUI_TRANSFER_UPLOAD_H

#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QDataStream>

#include "core/transfer.h"
#include "gui/style.h"
#include "gui/transfer_list.h"

namespace Transfer {

class UploadOld : public Item {
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
		Transfering,
		Finished
	};

	static constexpr qint64 max_send_queue_size = 1 << 13; // 8K, two pages

	Status status{Error};
	QString error{"init"};

	const Peer peer;
	const QString our_username;
	QTcpSocket socket;
	QDataStream socket_stream;
	qint64 next_message_size{-1};
	qint64 bytes_sent{0};

	QString filename;
	QFile file;

public:
	UploadOld (const Peer & peer, const QString & filepath, const QString & our_username,
	        QObject * parent = nullptr)
	    : Item (parent),
	      peer (peer),
	      our_username (our_username),
	      filename (QFileInfo (filepath).fileName ()),
	      file (filepath) {
		socket_stream.setVersion (Const::serializer_version);

		// Socket
		connect (&socket,
		         static_cast<void (QTcpSocket::*) (QTcpSocket::SocketError)> (&QTcpSocket::error), this,
		         &UploadOld::on_socket_error);
		connect (&socket, &QTcpSocket::connected, this, &UploadOld::on_connected);
		connect (&socket, &QTcpSocket::readyRead, this, &UploadOld::data_available);

		initiate_connection ();
	}

	/* Model funcs
	 */
private:
	void set_status (Status new_status) {
		status = new_status;
		emit data_changed (StatusField);
	}

	void failure (const QString & reason) {
		error = reason;
		set_status (Error);
		socket.abort ();
		socket_stream.resetStatus ();
		// TODO emit failed (reason);
	}

	QVariant data (int field, int role) const Q_DECL_OVERRIDE {
		switch (field) {
		case FilenameField: {
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
		case PeerField: {
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
		case SizeField: {
			// File size
			switch (role) {
			case Qt::DisplayRole:
				return size_to_string (file.size ());
			case Qt::StatusTipRole:
			case Qt::ToolTipRole:
				return tr ("%1B").arg (file.size ());
			}
		} break;
		case ProgressField: {
			// Transfer progress in %, and in bytes for tooltip
			switch (role) {
			case Qt::DisplayRole:
				if (status < Transfering)
					return 0;
				else if (status == Transfering && file.size () > 0)
					return int((100 * bytes_sent) / file.size ());
				else
					return 100;
			case Qt::StatusTipRole:
			case Qt::ToolTipRole:
				return QString ("%1/%2")
				    .arg (size_to_string (bytes_sent))
				    .arg (size_to_string (file.size ()));
			}
		} break;
		case StatusField: {
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
				case Finished:
					return tr ("Transfer complete");
				}
			} else if (role == Item::ButtonRole) {
				return int (Item::DeleteButton);
			}
		} break;
		}
		return {};
	}

	QVariant compare_data (int field) const Q_DECL_OVERRIDE {
		switch (field) {
		case SizeField:
			return qint64 (file.size ());
		case StatusField:
			return int(status);
		};
		return Item::compare_data (field);
	}

	void button_clicked (int field, Button btn) Q_DECL_OVERRIDE {
		(void) field;
		if (btn == Item::DeleteButton) {
			socket.abort ();
			deleteLater ();
		}
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
		case Transfering:
		case Finished:
			break;
		}
	}

	void socket_data_written (qint64 written) {
		bytes_sent += written;
		emit data_changed (ProgressField);
		if (bytes_sent == file.size ()) {
			socket.disconnectFromHost ();
			file.close ();
			set_status (Finished);
			return;
		}
		if (!file.atEnd ()) {
			auto chunk = file.read (max_send_queue_size - socket.bytesToWrite ());
			socket.write (chunk);
		}
	}

private:
	void initiate_connection (void) {
		set_status (Connecting);
		socket.connectToHost (peer.address, peer.port);
	}

	void start_transfering (void) {
		set_status (Transfering);
		emit data_changed (ProgressField);
		connect (&socket, &QTcpSocket::bytesWritten, this, &UploadOld::socket_data_written);
		if (!file.open (QIODevice::ReadOnly))
			failure (tr ("Cannot open file:") + file.errorString ());
		socket_data_written (0);
	}

	template <typename Msg> void send_message (const Msg & msg) {
		auto s = serialized_info.compute_size(Msg::code (), msg);
		Q_ASSERT (s <= Message::max_size);
		socket_stream << static_cast<Message::SizePrefixType> (s) << Msg::code () << msg;
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
		if (socket.bytesAvailable () < serialized_info.handshake_size)
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
		send_message (MessageOld::Offer{our_username, filename, file.size ()});
		return true;
	}

	bool da_waiting_request_answer (void) {
		// Read message size
		if (next_message_size == -1) {
			if (socket.bytesAvailable () < serialized_info.message_size_prefix_size)
				return false;
			Message::SizePrefixType s;
			socket_stream >> s;
			if (!check_datastream ())
				return false;
			next_message_size = static_cast<qint64> (s);
		}
		// Read message code then message itself (all interruption are errors)
		if (socket.bytesAvailable () < next_message_size)
			return false;
		next_message_size = -1;
		Message::CodeType code;
		socket_stream >> code;
		if (!check_datastream ())
			return false;
		switch (code) {
		case MessageOld::Accept::code ():
			// No data to read
			start_transfering ();
			return true;
		case MessageOld::Reject::code ():
			// No data to read
			failure (tr ("Transfer canceled by peer"));
			return false;
		default:
			failure (tr ("Protocol error"));
			return false;
		}
	}
};
}

#endif
