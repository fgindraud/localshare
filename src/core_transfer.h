#pragma once
#ifndef CORE_TRANSFER_H
#define CORE_TRANSFER_H

#include <QAbstractSocket>
#include <QDataStream>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QTimer>
#include <deque>
#include <limits>
#include <tuple>
#include <type_traits>

#include "core_localshare.h"
#include "core_payload.h"

namespace Transfer {

namespace Message {
	/* Protocol high level definition.
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

/* Implements the rate and progress notifications.
 * Signals:
 * - signals progress (bytes, files completed)
 * - transfer rate (overall and instantaneous)
 *
 * Overall rate is available at the end of computation.
 *
 * Progression is signaled by the progressed() signal.
 * This signal is triggered by send or receive.
 * A timer is used to limit the rate of signal emission (prevent too many Gui/Cli redraws).
 *
 * Instant byte rate is more complex.
 * It logs progress points (with a window to prevent using too much memory).
 * Instant rate is computed from the window (to average a bit).
 * It is emitted using instant_rate().
 * When progressed() are frequent enough, we emit instant_rate() before each of them with a flag.
 * This lets watching qobject wait for the progressed() signal before redrawing.
 * If progressed() is infrequent, instant_rate is emitted with a slow timer.
 */
class Notifier : public QObject {
	Q_OBJECT
private:
	// transfer total duration (for overall rate)
	QElapsedTimer transfer_timer;
	qint64 transfer_duration_msec{0};

	// progressed() rate limiter
	QElapsedTimer progress_timer;

	// instant rate (window buffer and timer for updates)
	struct Progress {
		qint64 epoch;
		qint64 transfered;
	};
	std::deque<Progress> history;
	QTimer update_rate_timer;

public:
	const Payload::Manager & payload;

signals:
	void progressed (void);
	void instant_rate (qint64 bytes_per_second, bool followed_by_progressed);

public:
	Notifier (const Payload::Manager & payload) : payload (payload) {
		connect (&update_rate_timer, &QTimer::timeout, this, &Notifier::update_rate);
	}

	void transfer_start (void) {
		transfer_timer.start ();
		progress_timer.start ();
		update_history ();
		update_rate_timer.start (Const::rate_update_interval_msec);
	}
	void transfer_end (void) {
		update_rate_timer.stop ();
		transfer_duration_msec = transfer_timer.elapsed ();
		emit progressed ();
		history.clear ();
	}
	void may_progress (void) {
		update_history ();
		if (progress_timer.elapsed () >= Const::progress_update_interval_msec) {
			progress_timer.start ();
			output_instant_rate (true);
			update_rate_timer.start (); // restart timer
			emit progressed ();
		}
	}

	// After end only

	qint64 get_transfer_time (void) const {
		Q_ASSERT (payload.is_transfer_complete ());
		return qMax (transfer_duration_msec, qint64 (1));
	}
	qint64 get_average_rate (void) const {
		return (payload.get_total_size () * 1000) / get_transfer_time ();
	}

private slots:
	void update_rate (void) {
		update_history ();
		output_instant_rate (false);
	}

private:
	void update_history (void) {
		// Move window
		auto epoch = transfer_timer.elapsed ();
		history.push_back ({epoch, payload.get_total_transfered_size ()});
		auto threshold = epoch - Const::progress_history_window_msec;
		while (history.front ().epoch < threshold &&
		       history.size () >= Const::progress_history_window_elem)
			history.pop_front ();
	}
	void output_instant_rate (bool followed_by_progressed) {
		if (history.size () < 2)
			return; // Not enough elements to compute a difference
		auto delta_bytes = history.back ().transfered - history.front ().transfered;
		auto delta_msec = history.back ().epoch - history.front ().epoch;
		auto rate = (1000 * delta_bytes) / qMax (delta_msec, qint64 (1));
		emit instant_rate (rate, followed_by_progressed);
	}
};

/* Transfer object base class.
 *
 * This class provides the implementation of protocol primitives.
 * It performs pre-protocol magic+ver verification ("handshake").
 * It will then parse the [code] or [code, size, <serialized content>] stream of messages.
 * Message handlers will be called when a message has been received.
 * Functions to send/receive messages are provided.
 * Includes:
 * - error reporting (calling failure/protocol_error)
 * - notifications for gui/cli (see Notifier)
 */
class Base : public QObject {
	Q_OBJECT

private:
	enum Status { WaitingForHandshake, WaitingForCode, WaitingForSize, WaitingForContent };
	Status status{WaitingForHandshake};
	Message::CodeType next_msg_code;
	Message::SizePrefixType next_msg_size;
	QString error;

	QAbstractSocket * socket;
	QDataStream stream;

protected:
	enum FailureMode {
		AbortMode,             // Critical, abort connection
		CloseMode,             // Close socket gracefully
		SendNoticeAndCloseMode // Send Error msg and close gracefully
	};
	Payload::Manager payload;
	Notifier notifier;
	QString peer_username;

signals:
	void failed (void);

public:
	Base (QAbstractSocket * socket_, const QString & peer_username, QObject * parent = nullptr)
	    : QObject (parent),
	      socket (socket_),
	      stream (socket),
	      notifier (payload),
	      peer_username (peer_username) {
		socket->setParent (this);
		stream.setVersion (Const::serializer_version);
		connect (socket, static_cast<void (QAbstractSocket::*) (QAbstractSocket::SocketError)> (
		                     &QAbstractSocket::error),
		         this, &Base::on_socket_error);
		connect (socket, &QAbstractSocket::connected, this, &Base::on_socket_connected);
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
	const Notifier * get_notifier (void) const { return &notifier; }
	Notifier * get_notifier (void) { return &notifier; }

private slots:
	void on_socket_error (void) {
		failure (tr ("Network error: %1").arg (socket->errorString ()), AbortMode);
	}
	void on_data_received (void) {
		if (status == WaitingForHandshake && !receive_handshake ())
			return;
		QElapsedTimer timer;
		timer.start ();
		while (receive_message ()) {
			if (timer.elapsed () > Const::max_work_msec) {
				// Return to event loop (but schedule this handler again)
				QTimer::singleShot (0, this, SLOT (on_data_received ()));
				break;
			}
		}
	}

protected slots:
	void on_socket_connected (void) { send_handshake (); }
	virtual void on_data_written (void) {}

protected:
	// Socket management

	void open_connection (const QHostAddress & address, quint16 port) {
		socket->connectToHost (address, port);
	}
	void close_connection (void) {
		socket->flush ();
		socket->disconnectFromHost ();
	}
	qint64 write_buffer_size (void) const { return socket->bytesToWrite (); }

	// Error reporting

	void failure (const QString & reason, FailureMode mode = SendNoticeAndCloseMode) {
		// For failures that are printed to users
		error = reason;
		if (mode == SendNoticeAndCloseMode)
			send_content_message (Message::Error, error);
		if (mode == AbortMode) {
			socket->abort ();
		} else {
			close_connection ();
		}
		payload.stop_transfer ();
		notifier.transfer_end ();
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

	// Message event Handlers

	virtual void on_handshake_completed (void) = 0;
	// Bool event handlers should return false to stop further processing of messages
	virtual bool on_receive_accept (void) = 0;
	virtual bool on_receive_reject (void) = 0;
	virtual bool on_receive_completed (void) = 0;
	// Event handlers of messages with content are called when content is buffered
	virtual bool on_receive_offer (void) = 0;
	virtual bool on_receive_chunk (void) = 0;
	virtual bool on_receive_checksums (void) = 0;

	// Protocol interaction utilities

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
		notifier.may_progress ();
		return true;
	}
	bool receive_next_chunk (void) {
		Q_ASSERT (next_msg_size > 0);
		if (!payload.receive_chunk (stream, next_msg_size)) {
			failure (tr ("Receive chunk error: %1").arg (payload.get_last_error ()));
			return false;
		}
		if (!check_stream ())
			return false;
		notifier.may_progress ();
		return true;
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
		notifier.may_progress ();
		return true;
	}

private:
	// Basic message primitives

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
			failure (tr ("Protocol version mismatch: %1 vs %2").arg (version, Const::protocol_version));
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
	enum Status { Error, Init, Starting, WaitingForPeerAnswer, Transfering, Completed, Rejected };

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

	bool set_payload (const QString & file_path_to_send, bool send_hidden_files) {
		// TODO setting for hidden files ?
		if (!payload.from_source_path (file_path_to_send, !send_hidden_files)) {
			failure (tr ("Cannot get file information: %1").arg (payload.get_last_error ()), AbortMode);
			return false;
		}
		return true;
	}

	void connect (const QHostAddress & address, quint16 port) {
		Q_ASSERT (payload.get_type () != Payload::Manager::Invalid);
		open_connection (address, port);
		set_status (Starting);
	}

	Status get_status (void) const { return status; }

private:
	void set_status (Status new_status) {
		status = new_status;
		emit status_changed (new_status);
	}
	bool refill_send_buffer (void) {
		QElapsedTimer timer;
		timer.start ();
		while (write_buffer_size () < Const::write_buffer_size &&
		       payload.get_total_transfered_size () < payload.get_total_size ()) {
			if (!send_next_chunk ())
				return false;
			if (timer.elapsed () > Const::max_work_msec)
				return true; // Return to event loop
		}
		return true;
	}
	void on_data_written (void) Q_DECL_OVERRIDE {
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
		payload.start_transfer (Payload::Manager::Sending);
		notifier.transfer_start ();
		set_status (Transfering);
		return refill_send_buffer ();
	}
	bool on_receive_reject (void) Q_DECL_OVERRIDE {
		if (status != WaitingForPeerAnswer) {
			protocol_error ("Reject when not WaitingForPeerAnswer");
			return false;
		}
		close_connection ();
		set_status (Rejected);
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
		notifier.transfer_end ();
		close_connection ();
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
	enum Status {
		Error,
		Starting,
		WaitingForOffer,
		WaitingForUserChoice,
		Transfering,
		Completed,
		Rejected
	};
	enum UserChoice { Accept, Reject };

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
		if (choice == Accept) {
			if (!send_code_message (Message::Accept))
				return;
			payload.start_transfer (Payload::Manager::Receiving);
			notifier.transfer_start ();
			set_status (Transfering);
		} else {
			send_code_message (Message::Reject);
			close_connection ();
			set_status (Rejected);
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
			notifier.transfer_end ();
			close_connection ();
			set_status (Completed);
		}
		return true;
	}
};
}

#endif
