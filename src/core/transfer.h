#pragma once
#ifndef CORE_TRANSFER_H
#define CORE_TRANSFER_H

#include <QAbstractSocket>
#include <QByteArray>
#include <QDataStream>
#include <QTcpSocket>
#include <limits>
#include <tuple>
#include <type_traits>

#include "core/localshare.h"
#include "core/payload.h"

namespace Transfer {

namespace Message {
	/* Protocol definition.
	 *
	 * Protocol:
	 *
	 * Uploader         Downloader
	 * ---[open connection]--->
	 * ---[magic+ver]--->
	 * <---[magic+ver]---
	 * IF (magic/ver doesn't match) { abort () }
	 * ---[offer]--->
	 * IF (accepted) {
	 * <---[accepted]---
	 * ---[chunks/checksums]--->
	 * <--[completed]---
	 * } ELSE  {
	 * <---[rejected]---
	 * }
	 * close () -- close ()
	 */

	/* All messages (except the initial handshake) are prefixed with a code to identify them.
	 * The high byte of the code is the protocol version.
	 * This ensures that stuff will break early if versions mismatch =)
	 */
	using CodeType = quint16;
	constexpr CodeType base_code = Const::protocol_version << 4;
	enum Code : CodeType {
		Error = base_code + 0, // +QString(error)
		Offer = base_code + 1, // +QString(our_username),Payload(file_list)
		Accept = base_code + 2,
		Reject = base_code + 3,
		Chunk = base_code + 4,     // >Manual transfer...
		Checksums = base_code + 5, // +Payload::Manager::ChecksumList
		Completed = base_code + 6
	};

	/* Messages with variable size content will be prefixed by their size (after code).
	 * This allow me to check that there are enough bytes buffered before deserializing.
	 */
	using SizePrefixType = quint32;
	constexpr auto max_size = static_cast<qint64> (std::numeric_limits<SizePrefixType>::max ());
}

namespace MessageOld {
	struct Offer {
		// Request for sending a file by the Uploader
		static constexpr Message::CodeType code (void) { return 0x42; }
		QString username; // Sender
		QString filename; // Name of file
		qint64 size;
	};
	inline QDataStream & operator<< (QDataStream & stream, const Offer & req) {
		return stream << req.username << req.filename << req.size;
	}
	inline QDataStream & operator>> (QDataStream & stream, Offer & req) {
		return stream >> req.username >> req.filename >> req.size;
	}

	struct Accept {
		// Accept the file offer
		static constexpr Message::CodeType code (void) { return 0x43; }
		// No data needed
	};
	inline QDataStream & operator<< (QDataStream & stream, const Accept &) { return stream; }
	inline QDataStream & operator>> (QDataStream & stream, Accept &) { return stream; }

	struct Reject {
		// Reject the file offer
		static constexpr Message::CodeType code (void) { return 0x44; }
		// No data needed
	};
	inline QDataStream & operator<< (QDataStream & stream, const Reject &) { return stream; }
	inline QDataStream & operator>> (QDataStream & stream, Reject &) { return stream; }
}

// Information on size of serialized structures
class Serialized {
private:
	/* This helper class allow to measure size of serialized data.
	 * It uses DummyDevice, a device that just counts the amount of bytes written.
	 */
	class MeasurementDevice {
	private:
		class DummyDevice : public QIODevice {
		public:
			DummyDevice (QObject * parent = nullptr) : QIODevice (parent) { open (QIODevice::WriteOnly); }

		protected:
			qint64 writeData (const char *, qint64 max_size) { return max_size; }
			qint64 readData (char *, qint64) { return -1; }
		};

	private:
		DummyDevice device;
		QDataStream stream;

	public:
		MeasurementDevice () {
			stream.setDevice (&device);
			stream.setVersion (Const::serializer_version);
		}

		template <typename... Args> qint64 compute_size (const Args &... args) {
			device.reset ();
			to_stream (stream, args...);
			Q_ASSERT (stream.status () == QDataStream::Ok);
			return device.pos ();
		}
	};

private:
	MeasurementDevice device;

public:
	// Precomputed sizes
	const qint64 handshake_size;
	const qint64 message_code_size;
	const qint64 message_size_prefix_size;

public:
	Serialized ()
	    : handshake_size (compute_size (Const::protocol_magic, Const::protocol_version)),
	      message_code_size (compute_size (Message::CodeType ())),
	      message_size_prefix_size (compute_size (Message::SizePrefixType ())) {}

	template <typename... Args> qint64 compute_size (const Args &... args) {
		return device.compute_size (args...);
	}
};
extern Serialized serialized_info; // Global precomputed size info (defined in main.cpp)

///////////////////////////////////

/* Transfer object base class.
 *
 * This class provides the implementation of protocol primitives.
 * It performs pre-protocol magic+ver verification ("handshake").
 * It will then parse the [code] or [code, size, <serialized content>] stream of messages.
 * Message handlers will be called when a message has been received.
 * Functions to send/receive messages are provided.
 * It centralizes error reporting.
 *
 * TODO progress indicator
 * TODO rate computation (instant, overall) ?
 * TODO limit read buffer size
 */
class Base : public QObject {
	Q_OBJECT

private:
	enum Status { WaitingForHandshake, WaitingForCode, WaitingForSize, WaitingForContent };
	Status status{WaitingForHandshake};
	Message::CodeType next_msg_code;
	Message::SizePrefixType next_msg_size;

	QString error;

protected:
	enum FailureMode {
		AbortMode,             // Critical, abort connection
		CloseMode,             // Close socket gracefully
		SendNoticeAndCloseMode // Send Error msg and close gracefully
	};
	Payload::Manager payload;
	QAbstractSocket * socket;
	QDataStream stream;

	QString peer_username;

signals:
	void failed (void);

public:
	Base (QAbstractSocket * socket_, const QString & peer_username, QObject * parent = nullptr)
	    : QObject (parent), socket (socket_), stream (socket), peer_username (peer_username) {
		socket->setParent (this);
		stream.setVersion (Const::serializer_version);
		connect (socket, static_cast<void (QAbstractSocket::*) (QAbstractSocket::SocketError)> (
		                     &QAbstractSocket::error),
		         this, &Base::on_socket_error);
		connect (socket, &QAbstractSocket::connected, this, &Base::on_socket_connected);
		connect (socket, &QAbstractSocket::disconnected, this, &Base::on_socket_disconnected);
		connect (socket, &QAbstractSocket::readyRead, this, &Base::on_data_received);
		connect (socket, &QAbstractSocket::bytesWritten, this, &Base::on_data_written);
	}
	Base (QAbstractSocket * socket, QObject * parent = nullptr) : Base (socket, QString (), parent) {}

	QString get_error (void) const { return error; }

	QString get_peer_username (void) const { return peer_username; }
	QString get_connection_info (void) const {
		return tr ("%1 on port %2").arg (socket->peerAddress ().toString ()).arg (socket->peerPort ());
	}
	const Payload::Manager & get_payload (void) const { return payload; }

private slots:
	void on_socket_error (void) {
		failure (tr ("Network error: %1").arg (socket->errorString ()), AbortMode);
	}
	void on_data_received (void) {
		if (status == WaitingForHandshake && !receive_handshake ())
			return;
		while (receive_message ()) {
		}
	}

protected slots:
	void on_socket_connected (void) { send_handshake (); }
	virtual void on_socket_disconnected (void) {}
	virtual void on_data_written (void) {}

protected:
	void failure (const QString & reason, FailureMode mode = SendNoticeAndCloseMode) {
		// For failures that are printed to users
		error = reason;
		if (mode == SendNoticeAndCloseMode)
			send_content_message (Message::Error, error);
		if (mode == AbortMode) {
			socket->abort ();
		} else {
			socket->flush ();
			socket->disconnectFromHost ();
		}
		payload.stop_transfer ();
		emit failed ();
	}
	void protocol_error (const char * details) {
		// Internal failures (or attack)
		qWarning ("Protocol error: %s", details);
		failure (tr ("Protocol error"), AbortMode);
	}
	void protocol_error (const QString & details) { protocol_error (qUtf8Printable (details)); }

	bool check_stream (void) {
		switch (stream.status ()) {
		case QDataStream::Ok:
			return true;
		case QDataStream::ReadPastEnd:
			protocol_error ("QDataStream: ReadPastEnd");
			return false;
		case QDataStream::ReadCorruptData:
			protocol_error ("QDataStream: corrupt data");
			return false;
		case QDataStream::WriteFailed:
			failure (tr ("Sending data failed: %1").arg (socket->errorString ()), AbortMode);
			return false;
		default:
			Q_UNREACHABLE ();
			return false;
		}
	}

	virtual void on_handshake_completed (void) = 0;
	// Bool event handlers should return false to stop further processing of messages
	virtual bool on_receive_accept (void) = 0;
	virtual bool on_receive_reject (void) = 0;
	virtual bool on_receive_completed (void) = 0;
	// Event handlers of messages with content are called when content is buffered
	virtual bool on_receive_offer (void) = 0;
	virtual bool on_receive_chunk (void) = 0;
	virtual bool on_receive_checksums (void) = 0;

	bool send_code_message (Message::Code code) {
		stream << Message::CodeType (code);
		return check_stream ();
	}

	bool send_offer (const QString & our_username) {
		return send_content_message (Message::Offer, std::tie (our_username, payload));
	}
	bool receive_offer (void) {
		stream >> std::tie (peer_username, payload);
		if (!check_stream ())
			return false;
		if (!payload.validate ()) {
			failure (tr ("Peer offer is invalid: %1").arg (payload.get_last_error ()), AbortMode);
			return false;
		}
		return true;
	}

	bool send_next_chunk (void) {
		auto size = payload.next_chunk_size ();
		Q_ASSERT (size > 0); // Should not be called if no more chunks
		Q_ASSERT (size <= Message::max_size);
		stream << Message::CodeType (Message::Chunk) << Message::SizePrefixType (size);
		if (!payload.send_next_chunk (stream)) {
			failure (tr ("Send chunk error: %1").arg (payload.get_last_error ()));
			return false;
		}
		if (!check_stream ())
			return false;
		// Send checksums if any
		auto checksums = payload.take_pending_checksums ();
		if (!checksums.empty ())
			return send_content_message (Message::Checksums, checksums);
		return true;
	}
	bool receive_next_chunk (void) {
		Q_ASSERT (next_msg_size > 0);
		if (!payload.receive_chunk (stream, next_msg_size)) {
			failure (tr ("Receive chunk error: %1").arg (payload.get_last_error ()));
			return false;
		}
		return check_stream ();
	}
	bool receive_checksums (void) {
		Payload::Manager::ChecksumList checksums;
		stream >> checksums;
		if (!check_stream ())
			return false;
		if (!payload.test_checksums (checksums)) {
			failure (payload.get_last_error ());
			return false;
		}
		return true;
	}

private:
	bool send_handshake (void) {
		stream << std::tie (Const::protocol_magic, Const::protocol_version);
		return check_stream ();
	}
	bool receive_handshake (void) {
		// Returns true if can continue to receive stuff
		if (socket->bytesAvailable () < serialized_info.handshake_size)
			return false;
		std::remove_const<decltype (Const::protocol_magic)>::type magic;
		std::remove_const<decltype (Const::protocol_version)>::type version;
		stream >> std::tie (magic, version);
		if (!check_stream ())
			return false;
		if (magic != Const::protocol_magic) {
			protocol_error ("Magic check failed");
			return false;
		}
		if (version != Const::protocol_version) {
			failure (
			    tr ("Protocol version mismatch: %1 vs %2").arg (version).arg (Const::protocol_version));
			return false;
		}
		status = WaitingForCode;
		on_handshake_completed ();
		return true;
	}

	template <typename Msg> bool send_content_message (Message::Code code, const Msg & msg) {
		auto size = serialized_info.compute_size (msg);
		Q_ASSERT (size < Message::max_size);
		stream << Message::CodeType (code) << Message::SizePrefixType (size) << msg;
		return check_stream ();
	}
	bool receive_message (void) {
		// Returns true if can continue to receive stuff
		if (status == WaitingForCode) {
			if (socket->bytesAvailable () < serialized_info.message_code_size)
				return false;
			stream >> next_msg_code;
			if (!check_stream ())
				return false;
			switch (next_msg_code) {
			// After: get size
			case Message::Error:
			case Message::Offer:
			case Message::Chunk:
			case Message::Checksums:
				status = WaitingForSize;
				break;
			// After : get next message code
			case Message::Accept:
				return on_receive_accept ();
			case Message::Reject:
				return on_receive_reject ();
			case Message::Completed:
				return on_receive_completed ();
			default:
				protocol_error (QString ("Unknown message type: %1").arg (next_msg_code, 0, 16));
				return false;
			}
		}
		if (status == WaitingForSize) {
			if (socket->bytesAvailable () < serialized_info.message_size_prefix_size)
				return false;
			stream >> next_msg_size;
			if (!check_stream ())
				return false;
			if (next_msg_size <= 0) {
				protocol_error ("Next message size <= 0");
				return false;
			}
			status = WaitingForContent;
		}
		if (status == WaitingForContent) {
			if (socket->bytesAvailable () < next_msg_size)
				return false;
			switch (next_msg_code) {
			case Message::Error: {
				// After : nothing
				QString error_msg;
				stream >> error_msg;
				if (check_stream ())
					failure (tr ("Peer failed with: %1").arg (error_msg), CloseMode);
				return false;
			}
			// After : get next message code
			case Message::Offer:
				status = WaitingForCode;
				return on_receive_offer ();
			case Message::Chunk:
				status = WaitingForCode;
				return on_receive_chunk ();
			case Message::Checksums:
				status = WaitingForCode;
				return on_receive_checksums ();
			default:
				Q_UNREACHABLE ();
				return false;
			}
		}
		return true;
	}
};

/* Upload class.
 * Split initialization (start), to allow catching files search errors.
 * Can be displayed from the beginning (after start).
 */
class Upload : public Base {
	Q_OBJECT

public:
	enum Status { Error, Init, Starting, WaitingForPeerAnswer, Transfering, Completed };

private:
	const QString our_username;
	Status status;

signals:
	void status_changed (Status new_status);

public:
	Upload (const QString & peer_username, const QString & our_username, QObject * parent = nullptr)
	    : Base (new QTcpSocket, peer_username, parent), our_username (our_username), status (Init) {
		QObject::connect (this, &Base::failed, [this] { set_status (Error); });
	}

	bool set_payload (const QString & file_path_to_send) {
		// TODO setting for hidden files ?
		if (!payload.from_source_path (file_path_to_send, true)) {
			failure (tr ("Cannot get file information: %1").arg (payload.get_last_error ()), AbortMode);
			return false;
		}
		return true;
	}

	void connect (const QHostAddress & address, quint16 port) {
		Q_ASSERT (payload.get_type () != Payload::Manager::Invalid);
		socket->connectToHost (address, port);
		set_status (Starting);
	}

	Status get_status (void) const { return status; }

private:
	void set_status (Status new_status) {
		status = new_status;
		emit status_changed (new_status);
	}
	bool refill_send_buffer (void) {
		while (socket->bytesToWrite () < Const::write_buffer_size &&
		       payload.get_total_transfered_size () < payload.get_total_size ()) {
			if (!send_next_chunk ())
				return false;
		}
		return true;
	}
	void on_data_written (void) {
		if (status == Transfering)
			refill_send_buffer ();
	}

	void on_handshake_completed (void) Q_DECL_OVERRIDE {
		Q_ASSERT (status == Starting);
		if (send_offer (our_username))
			set_status (WaitingForPeerAnswer);
	}
	bool on_receive_accept (void) Q_DECL_OVERRIDE {
		if (status != WaitingForPeerAnswer) {
			protocol_error ("Accept when not WaitingForPeerAnswer");
			return false;
		}
		set_status (Transfering);
		payload.start_transfer (Payload::Manager::Sending);
		return refill_send_buffer ();
	}
	bool on_receive_reject (void) Q_DECL_OVERRIDE {
		if (status != WaitingForPeerAnswer) {
			protocol_error ("Reject when not WaitingForPeerAnswer");
			return false;
		}
		failure (tr ("Transfer rejected by peer"), CloseMode);
		return false;
	}
	bool on_receive_completed (void) Q_DECL_OVERRIDE {
		if (status != Transfering) {
			protocol_error ("Completed when not Transfering");
			return false;
		}
		if (!payload.is_transfer_complete ()) {
			protocol_error ("Transfer not complete on sender");
			return false;
		}
		socket->disconnectFromHost ();
		set_status (Completed);
		return true;
	}
	bool on_receive_offer (void) Q_DECL_OVERRIDE {
		protocol_error ("Offer in Upload");
		return false;
	}
	bool on_receive_chunk (void) Q_DECL_OVERRIDE {
		protocol_error ("Chunk in Upload");
		return false;
	}
	bool on_receive_checksums (void) Q_DECL_OVERRIDE {
		protocol_error ("Checksums in Upload");
		return false;
	}
};

/* Download class.
 * Cannot be displayed at first due to incomplete data.
 * Can be displayed when status goes to WaitingForUserChoice.
 * Automatic download should be supported externally.
 */
class Download : public Base {
	Q_OBJECT

public:
	enum Status { Error, Starting, WaitingForOffer, WaitingForUserChoice, Transfering, Completed };
	enum UserChoice { Accepted, Rejected };

private:
	Status status;

signals:
	void status_changed (Status new_status);

public:
	Download (QAbstractSocket * socket, QObject * parent = nullptr)
	    : Base (socket, parent), status (Starting) {
		on_socket_connected ();
		connect (this, &Base::failed, [this] { set_status (Error); });
	}

	Status get_status (void) const { return status; }

	void set_target_dir (const QString & path) {
		Q_ASSERT (status == WaitingForUserChoice);
		payload.set_root_dir (path);
	}
	void give_user_choice (UserChoice choice) {
		Q_ASSERT (status == WaitingForUserChoice);
		if (choice == Accepted) {
			if (!send_code_message (Message::Accept))
				return;
			payload.start_transfer (Payload::Manager::Receiving);
			set_status (Transfering);
		} else {
			send_code_message (Message::Reject);
			failure (tr ("Transfer refused"), CloseMode);
		}
	}

private:
	void set_status (Status new_status) {
		status = new_status;
		emit status_changed (new_status);
	}

	void on_handshake_completed (void) Q_DECL_OVERRIDE {
		Q_ASSERT (status == Starting);
		set_status (WaitingForOffer);
	}
	bool on_receive_accept (void) Q_DECL_OVERRIDE {
		protocol_error ("Accept in Download");
		return false;
	}
	bool on_receive_reject (void) Q_DECL_OVERRIDE {
		protocol_error ("Reject in Download");
		return false;
	}
	bool on_receive_completed (void) Q_DECL_OVERRIDE {
		protocol_error ("Completed in Download");
		return false;
	}
	bool on_receive_offer (void) Q_DECL_OVERRIDE {
		if (status != WaitingForOffer) {
			protocol_error ("Offer msg while not WaitingForOffer");
			return false;
		}
		if (!receive_offer ())
			return false;
		set_status (WaitingForUserChoice);
		return true;
	}
	bool on_receive_chunk (void) Q_DECL_OVERRIDE {
		if (status != Transfering) {
			protocol_error ("Chunk while not Transfering");
			return false;
		}
		return receive_next_chunk ();
	}
	bool on_receive_checksums (void) Q_DECL_OVERRIDE {
		if (status != Transfering) {
			protocol_error ("Checksums while not Transfering");
			return false;
		}
		if (!receive_checksums ())
			return false;
		if (payload.is_transfer_complete ()) {
			if (!send_code_message (Message::Completed))
				return false;
			socket->flush ();
			socket->disconnectFromHost ();
			set_status (Completed);
		}
		return true;
	}
};
}

#endif
