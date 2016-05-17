#ifndef TRANSFER_DOWNLOAD_H
#define TRANSFER_DOWNLOAD_H

#include <QTcpSocket>
#include <QDataStream>
#include <QFileDialog>

#include "style.h"
#include "settings.h"
#include "transfer.h"
#include "transfer_model.h"

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
		Transfering,
		Finished
	};

	Status status{Error};
	QString error{"init"};

	QAbstractSocket * socket;
	QDataStream socket_stream;
	qint64 next_message_size{-1};

	QString filename;
	QString filepath;
	qint64 size{-1};
	QString username;

	QFile outfile;

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

	void set_filename (const QString & new_filename, const QString & new_filepath) {
		filename = new_filename;
		filepath = new_filepath;
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
	}

	QVariant data (int field, int role) const Q_DECL_OVERRIDE {
		switch (field) {
		case FilenameField: {
			// Display filename and transfer direction
			switch (role) {
			case Qt::DisplayRole:
				return filename;
			case Qt::StatusTipRole:
			case Qt::ToolTipRole:
				if (filepath.isEmpty ())
					return tr ("Download");
				else
					return tr ("Downloading %1 to %2").arg (filename).arg (filepath);
			case Qt::DecorationRole:
				return Icon::download ();
			case Item::ButtonRole:
				if (status == WaitingUserChoice)
					return int (Item::ChangeDownloadPathButton);
				break;
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
				if (status < Transfering)
					return 0;
				else if (status == Transfering && size > 0)
					return int((100 * outfile.pos ()) / size);
				else
					return 100;
			case Qt::StatusTipRole:
			case Qt::ToolTipRole:
				return QString ("%1/%2").arg (size_to_string (outfile.size ())).arg (size_to_string (size));
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
					return tr ("Download file ?");
				case Transfering:
					return tr ("Transfering");
				case Finished:
					return tr ("Transfer complete");
				}
			} break;
			case Item::ButtonRole: {
				Item::Buttons btns = Item::DeleteButton;
				if (status == WaitingUserChoice)
					btns |= Item::AcceptButton | Item::CancelButton;
				return int (btns);
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
		if (btn == Item::DeleteButton) {
			socket->abort ();
			deleteLater ();
			return;
		}

		switch (status) {
		case WaitingUserChoice: {
			switch (btn) {
			case AcceptButton:
				start_transfering ();
				emit data_changed (FilenameField); // Removing button to change download filepath
				return;
			case CancelButton:
				send_message (Message::Reject{});
				socket->flush ();
				socket->close ();
				failure (tr ("Transfer cancelled"));
				return;
			case ChangeDownloadPathButton: {
				QString path =
				    QFileDialog::getSaveFileName (nullptr, tr ("Select download destination"), filepath);
				if (!path.isEmpty ())
					set_filename (filename, path);
				return;
			}
			default:
				break;
			}
		} break;
		default:
			break;
		}
		qWarning () << "unexpected: button" << static_cast<Buttons> (btn)
		            << "has been clicked in status" << status << "for field" << field;
	}

	/* Protocol implementation
	 */
private slots:
	void on_socket_error (QTcpSocket::SocketError) {
		failure (tr ("Network error: ") + socket->errorString ());
	}

	void data_available (void) {
		/* Non break switch, as we can move multiple steps if enough data arrived.
		 * At each step, data is managed by da_* functions, that return true only if we can go to the
		 * next step.
		 */
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
				start_transfering ();
			} else {
				set_status (WaitingUserChoice);
				emit data_changed (FilenameField); // Adding button to change download filepath
			}
		}
		case WaitingUserChoice: {
			/* We are not waiting for data in this step, so break.
			 * But do not break if we just skip this step to Transfering.
			 */
			if (status == WaitingUserChoice)
				break;
		}
		case Transfering:
			da_transfering ();
			break;
		case Finished:
			break;
		}
	}

private:
	void initiate_protocol (void) {
		set_status (WaitingHandshake);
		socket_stream << Const::protocol_magic << Const::protocol_version;
	}

	void start_transfering (void) {
		set_status (Transfering);
		outfile.setFileName (filepath);
		if (!outfile.open (QIODevice::WriteOnly)) {
			failure (tr ("Cannot open destination file: ") + outfile.errorString ());
			return;
		}
		send_message (Message::Accept{});
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
		// Receiving handshake
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
		set_filename (offer.filename, Settings::DownloadPath ().get () + "/" + offer.filename);
		set_size (offer.size);
		return true;
	}

	void da_transfering (void) {
		auto data = socket->read (socket->bytesAvailable ());
		outfile.write (data);
		emit data_changed (ProgressField);
		if (outfile.pos () == size) {
			socket->disconnectFromHost ();
			outfile.close ();
			set_status (Finished);
		}
	}
};
}

#endif
