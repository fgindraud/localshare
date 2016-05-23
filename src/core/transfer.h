#pragma once
#ifndef CORE_TRANSFER_H
#define CORE_TRANSFER_H

#include <QDataStream>
#include <QByteArray>
#include <limits>
#include <type_traits>

#include "core/localshare.h"

namespace Transfer {

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

	/* Transferred data is sent by chunks.
	 * TODO limit write queue size
	 */
	static constexpr qint64 default_chunk_size = quint64 (1) << 12; // A page

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

	struct DataChunk {
		// Transfer a data chunk
		static constexpr Code code (void) { return 0x45; }
		quint16 checksum;
		qint64 index;    // number of this chunk, starting at 0
		QByteArray data; // contain data size
	};
	inline QDataStream & operator<<(QDataStream & stream, const DataChunk & chunk) {
		return stream << chunk.checksum << chunk.index << chunk.data;
	}
	inline QDataStream & operator>>(QDataStream & stream, DataChunk & chunk) {
		return stream >> chunk.checksum >> chunk.index >> chunk.data;
	}
}
}

#endif
