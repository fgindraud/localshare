#pragma once
#ifndef CORE_PAYLOAD_H
#define CORE_PAYLOAD_H

#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QQueue>
#include <list>
#include <memory>

#include "core/localshare.h"

namespace Payload {
/* Represent a File in a payload.
 * file_path is relative to the payload root_dir and contains the file name.
 * It caches info from QFileInfo to check if it changed later.
 * It also acts as a kind a QIODevice for reading/writing data to the file.
 * In either mode it builds an hash of the file to allow a check later.
 *
 * Supported modes:
 * - ReadOnly: for the sender, will check the file has not changed
 * - ReadWrite: for the receiver, will create the path and file
 *
 * Note that WriteOnly for the received would not work:
 * - We need to read data to compare to checksums
 * - mmap (Shared, WriteOnly) fails on Linux...
 *
 * This class is neither copyable nor movable (due to QFile).
 * It is not a QObject as signals/slots of QFile are not useful.
 */
class File : public Streamable, public Debugable {
private:
	QString last_error;

	QString file_path;
	qint64 size;
	QDateTime last_modified;

	// QFile destructor will close file and mappings
	QFile file;
	char * mapping{nullptr};
	qint64 pos;
	QCryptographicHash hash{Const::hash_algorithm};

public:
	File () = default;
	File (const QFileInfo & file_info, const QString & rel_path)
	    : file_path (rel_path + "/" + file_info.fileName ()),
	      size (file_info.size ()),
	      last_modified (file_info.lastModified ()) {}

	QString get_last_error (void) const { return last_error; }
	bool at_end (void) const { return pos == size; }

	QString get_file_path (void) const { return file_path; }
	qint64 get_size (void) const { return size; }

	// Only export/import filename and size
	void to_stream (QDataStream & stream) const { stream << file_path << size; }
	void from_stream (QDataStream & stream) { stream >> file_path >> size; }
	QDebug to_debug (QDebug debug) const {
		QDebugStateSaver saver (debug);
		debug.nospace () << "File(" << file_path << ", " << size << ")";
		return debug;
	}

	// Hash export / import-check
	QByteArray get_checksum (void) const { return hash.result (); }
	bool test_checksum (const QByteArray & cs) {
		if (cs != hash.result ()) {
			last_error = QObject::tr ("Checksum does not match for file %1").arg (file_path);
			return false;
		} else {
			return true;
		}
	}

	// QIODevice similar open & close

	bool open (const QString & root_dir_path, QIODevice::OpenMode mode) {
		Q_ASSERT (mode == QIODevice::ReadOnly || mode == QIODevice::ReadWrite);
		QFileInfo info (root_dir_path + "/" + file_path);
		if (mode == QIODevice::ReadOnly) {
			// Check file didn't change
			if (info.size () != size || info.lastModified () != last_modified) {
				last_error = QObject::tr ("File %1 has changed").arg (file_path);
				return false;
			}
		} else if (mode == QIODevice::ReadWrite) {
			// Make path
			auto dir = info.dir ();
			if (!dir.mkpath (".")) {
				last_error = QObject::tr ("Unable to create path: %1").arg (dir.path ());
				return false;
			}
		}
		// Open and map memory
		file.setFileName (info.filePath ());
		if (!file.open (mode)) {
			last_error = QObject::tr ("Unable to open file %1: %2")
			                 .arg (info.filePath ())
			                 .arg (file.errorString ());
			return false;
		}
		if (mode == QIODevice::ReadWrite) {
			// Resize before mapping
			if (!file.resize (size)) {
				last_error = QObject::tr ("Unable to resize file %1: %2")
				                 .arg (info.filePath ())
				                 .arg (file.errorString ());
				return false;
			}
		}
		auto addr = file.map (0, size);
		if (addr == nullptr) {
			last_error = QObject::tr ("Unable to map file %1: %2")
			                 .arg (info.filePath ())
			                 .arg (file.errorString ());
			return false;
		}
		mapping = reinterpret_cast<char *> (addr);
		pos = 0;
		hash.reset ();
		return true;
	}

	bool is_open (void) const { return file.isOpen (); }

	void close (void) {
		if (mapping != nullptr) {
			file.unmap (reinterpret_cast<uchar *> (mapping));
			mapping = nullptr;
		}
		file.close ();
	}

	/* Read or write data to the file, to or from a QDataStream.
	 * bytes is the maximum amount of data to transfer.
	 * Both return bytes read/written, or -1 on error.
	 * Any error will come from the stream itself, so get_last_error() is not useful.
	 */

	qint64 read_data (QDataStream & target, qint64 bytes) {
		Q_ASSERT (mapping);
		auto p = &mapping[pos];
		auto bytes_read = target.writeRawData (p, qMin (bytes, size - pos));
		if (bytes_read > 0) {
			hash.addData (p, bytes_read);
			pos += bytes_read;
		}
		return bytes_read;
	}

	qint64 write_data (QDataStream & source, qint64 bytes) {
		Q_ASSERT (mapping);
		auto p = &mapping[pos];
		auto bytes_read = source.readRawData (p, qMin (bytes, size - pos));
		if (bytes_read > 0) {
			hash.addData (p, bytes_read);
			pos += bytes_read;
		}
		return bytes_read;
	}
};

/* Represent file and dirs.
 * Perform conversion between Dirs/files <-> data chunks (protocol)
 *
 * Is attached to a root dir (target or source).
 * It has a list of files handles (with paths relative to root).
 *
 * The sender will user next_chunk_size () and send_next_chunk () to send chunks until stop.
 * Call receive chunk with chunk size until total_transfered==total_size.
 *
 * This class never checks the status of the stream object.
 *
 * TODO ability to set a position (for restarts) ?
 * TODO user setReadBufferSize on sockets (limit for protection)
 */
class Manager : public Streamable, public Debugable {
public:
	enum Mode { Closed, Sending, Receiving };
	using Checksum = QByteArray;
	using ChecksumList = QList<Checksum>;

private:
	using FileList = std::list<File>; // std::list can handle File (non copyable/movable)

	QString last_error;

	// Transfer display information
	QString transfer_name;
	qint64 total_size{0};

	QString root_dir_path;
	FileList files;

	// Progress
	Mode transfer_status{Closed};
	FileList::iterator current_file;
	FileList::iterator next_file_to_checksum;
	qint64 total_transfered;

public:
	QString get_last_error (void) const { return last_error; }

	QString get_transfer_name (void) const { return transfer_name; }
	qint64 get_total_size (void) const { return total_size; }

	bool is_simple_file_transfer (void) const {
		return files.size () == 1 && QFileInfo (files.front ().get_file_path ()).path () == ".";
	}

	void set_root_dir (const QString & path) {
		Q_ASSERT (transfer_status == Closed);
		root_dir_path = path;
	}

	// File list management

	bool from_source_path (const QString & path, bool ignore_hidden) {
		Q_ASSERT (transfer_status == Closed);
		Q_ASSERT (total_size == 0); // Should only be called once
		auto cleaned_path = QFileInfo (path).canonicalFilePath ();
		if (cleaned_path.isEmpty ()) {
			last_error = QObject::tr ("Invalid path: %1").arg (path);
			return false;
		}
		QFileInfo path_info (cleaned_path);
		set_root_dir (path_info.path ());
		if (path_info.isFile ()) {
			files.emplace_back (path_info, ".");
			total_size += path_info.size ();
			transfer_name = path_info.fileName ();
			return true;
		} else if (path_info.isDir ()) {
			auto root_dir = path_info.dir ();
			auto filter_flags =
			    QDir::AllDirs | QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Readable;
			if (!ignore_hidden)
				filter_flags |= QDir::Hidden;
			root_dir.setFilter (filter_flags);
			root_dir.setSorting (QDir::Size | QDir::Reversed); // Small files first
			// Recursively search dirs for files
			QQueue<QString> relative_paths_to_search;
			relative_paths_to_search.enqueue (path_info.fileName ());
			while (!relative_paths_to_search.isEmpty ()) {
				auto rel_path = relative_paths_to_search.dequeue ();
				QDir dir{root_dir};
				if (!dir.cd (rel_path))
					continue;
				for (const auto & entry : dir.entryInfoList ()) {
					if (entry.isFile ()) {
						files.emplace_back (entry, rel_path);
						total_size += entry.size ();
					} else if (entry.isDir ()) {
						relative_paths_to_search.enqueue (rel_path + "/" + entry.fileName ());
					}
				}
			}
			if (files.empty ()) {
				last_error = QObject::tr ("No file found in directory: %1").arg (path);
				return false;
			}
			transfer_name = path_info.fileName () + "/";
			return true;
		} else {
			last_error = QObject::tr ("Path is neither a file nor a directory: %1").arg (path);
			return false;
		}
	}

	// Import/export. File class is not movable nor copyable, so extra care is needed.

	void to_stream (QDataStream & stream) const {
		stream << transfer_name << total_size << quint32 (files.size ());
		for (const auto & f : files)
			stream << f;
	}
	void from_stream (QDataStream & stream) {
		Q_ASSERT (transfer_status == Closed);
		Q_ASSERT (files.size () == 0); // Should only be called once
		quint32 c;
		stream >> transfer_name >> total_size >> c;
		files.clear ();
		for (quint32 i = 0; i < c; ++i) {
			files.emplace_back ();
			stream >> files.back ();
		}
	}
	QDebug to_debug (QDebug debug) const {
		QDebugStateSaver saver (debug);
		debug.nospace () << "Manager(" << transfer_name << ", " << total_size << ") {\n";
		for (const auto & f : files)
			debug << '\t' << f << '\n';
		return debug << '}';
	}

	// Send / receive next chunk and checksums

	void start_transfer (Mode mode) {
		Q_ASSERT (transfer_status == Closed);
		Q_ASSERT (mode != Closed);
		transfer_status = mode;
		total_transfered = 0;
		current_file = next_file_to_checksum = files.begin ();
	}

	qint64 get_total_transfered_size (void) const { return total_transfered; }

	qint64 next_chunk_size (void) const {
		// Chunk are all of size Const::chunk_size, except the last which is truncated
		// 0 means no more to transfer
		return qMin (Const::chunk_size, total_size - total_transfered);
	}

	bool send_next_chunk (QDataStream & stream) {
		Q_ASSERT (transfer_status == Sending);
		auto bytes_to_send = next_chunk_size ();
		while (bytes_to_send > 0) {
			Q_ASSERT (total_transfered <= total_size);
			Q_ASSERT (current_file != files.end ()); // Should stop due to size test
			if (!current_file->is_open () && !current_file->open (root_dir_path, QIODevice::ReadOnly)) {
				transfer_error (current_file->get_last_error ());
				return false;
			}
			auto sent = current_file->read_data (stream, bytes_to_send);
			if (sent == -1) {
				transfer_error (QObject::tr ("Unable to send data to socket: %1")
				                    .arg (stream.device ()->errorString ()));
				return false;
			}
			bytes_to_send -= sent;
			total_transfered += sent;
			if (current_file->at_end ()) {
				current_file->close ();
				current_file++;
			}
		}
		if (total_transfered == total_size) {
			Q_ASSERT (current_file == files.end ());
			transfer_status = Closed;
		}
		return true;
	}

	bool receive_chunk (QDataStream & stream, qint64 chunk_size) {
		Q_ASSERT (transfer_status == Receiving);
		auto bytes_to_receive = chunk_size;
		while (bytes_to_receive > 0) {
			Q_ASSERT (total_transfered <= total_size);
			Q_ASSERT (current_file != files.end ()); // Should stop due to size test
			if (!current_file->is_open () && !current_file->open (root_dir_path, QIODevice::ReadWrite)) {
				transfer_error (current_file->get_last_error ());
				return false;
			}
			auto received = current_file->write_data (stream, bytes_to_receive);
			if (received == -1) {
				transfer_error (QObject::tr ("Unable to received data from socket: %1")
				                    .arg (stream.device ()->errorString ()));
				return false;
			}
			bytes_to_receive -= received;
			total_transfered += received;
			if (current_file->at_end ()) {
				current_file->close ();
				current_file++;
			}
		}
		if (total_transfered == total_size) {
			Q_ASSERT (current_file == files.end ());
			transfer_status = Closed;
		}
		return true;
	}

	// Checksums

	ChecksumList take_pending_checksums (void) {
		ChecksumList checksums;
		// We can only send checksums if files have been processed
		for (; next_file_to_checksum != current_file; ++next_file_to_checksum)
			checksums.append (next_file_to_checksum->get_checksum ());
		return checksums;
	}

	bool test_checksums (const ChecksumList & checksums) {
		// Test checksums against files (must have been processed before)
		for (const auto & checksum : checksums) {
			if (next_file_to_checksum == current_file) {
				transfer_error (QObject::tr ("Received checksum of incomplete file."));
				return false;
			}
			if (!next_file_to_checksum->test_checksum (checksum)) {
				last_error = next_file_to_checksum->get_last_error ();
				return false;
			}
			next_file_to_checksum++;
		}
		return true;
	}

private:
	void transfer_error (const QString & why) {
		last_error = why;
		if (current_file != files.end ())
			current_file->close ();
		current_file = files.end ();
		transfer_status = Closed;
	}
};
}

#endif
