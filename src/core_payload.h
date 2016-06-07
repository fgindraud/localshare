/* Localshare - Small file sharing application for the local network.
 * Copyright (C) 2016 Francois Gindraud
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef CORE_PAYLOAD_H
#define CORE_PAYLOAD_H

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <list>
#include <memory>

#include "core_localshare.h"

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
 * 0 bytes files:
 * - mmap cannot be used on them
 * - most operations will be noop, and no mapping is performed
 *
 * This class is neither copyable nor movable (due to QFile).
 * It is not a QObject as signals/slots of QFile are not useful.
 */
class File : public Streamable {
	Q_DECLARE_TR_FUNCTIONS (File);

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
	File (const QFileInfo & file_info, const QDir & payload_dir)
	    : file_path (payload_dir.relativeFilePath (file_info.filePath ())),
	      size (file_info.size ()),
	      last_modified (file_info.lastModified ()) {}

	QString get_last_error (void) const { return last_error; }
	bool at_end (void) const { return pos == size; }

	QString get_relative_path (void) const { return file_path; }
	qint64 get_size (void) const { return size; }

	// Only export/import filename and size
	void to_stream (QDataStream & stream) const { stream << file_path << size; }
	void from_stream (QDataStream & stream) { stream >> file_path >> size; }
	bool validate_path (void) const {
		// Check path is not out of target dir tree
		return QDir::isRelativePath (file_path) && !file_path.contains ("..");
	}

	// Hash export / import-check
	QByteArray get_checksum (void) const { return hash.result (); }
	bool test_checksum (const QByteArray & cs) {
		if (cs != hash.result ()) {
			last_error = tr ("Checksum does not match for file %1").arg (file_path);
			return false;
		} else {
			return true;
		}
	}

	// QIODevice similar open & close

	bool open (const QDir & payload_dir, QIODevice::OpenMode mode) {
		Q_ASSERT (mode == QIODevice::ReadOnly || mode == QIODevice::ReadWrite);
		QFileInfo info (payload_dir.filePath (file_path));
		if (mode == QIODevice::ReadOnly) {
			// Check file didn't change
			if (info.size () != size || info.lastModified () != last_modified) {
				last_error = tr ("File %1 has changed").arg (file_path);
				return false;
			}
		} else if (mode == QIODevice::ReadWrite) {
			// Make path
			auto dir = info.dir ();
			if (!dir.mkpath (".")) {
				last_error = tr ("Unable to create path: %1").arg (dir.path ());
				return false;
			}
		}
		// Open and map memory
		file.setFileName (info.filePath ());
		if (!file.open (mode)) {
			last_error = tr ("Unable to open file %1: %2").arg (info.filePath (), file.errorString ());
			return false;
		}
		if (mode == QIODevice::ReadWrite) {
			// Resize before mapping
			if (size > 0 && !file.resize (size)) {
				last_error =
				    tr ("Unable to resize file %1: %2").arg (info.filePath (), file.errorString ());
				return false;
			}
		}
		if (size > 0) {
			auto addr = file.map (0, size);
			if (addr == nullptr) {
				last_error = tr ("Unable to map file %1: %2").arg (info.filePath (), file.errorString ());
				return false;
			}
			mapping = reinterpret_cast<char *> (addr);
		}
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
		if (size == 0)
			return 0;
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
		if (size == 0)
			return 0;
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
 * A payload is a set of files in a directory tree.
 * <payload_root> is the root of this directory tree (must be single dir name).
 * For a single file transfer, payload is one file at <payload_root> (which is ".").
 * A local <root_dir> indicates the position of the payload in the file system.
 *
 * Summary:
 * Path of file = <root_dir>/<payload_root>/<file_relative_path>
 * Payload is the set of <payload_root>/<file_relative_path> for each file.
 * <root_dir> is the local payload position (source or destination).
 * <payload_root> must be either a single dir_name or "."
 * get_payload_dir() returns the local <root_dir>/<payload_root>
 *
 * All file paths must stay in <root_dir>/<payload_root>.
 * Otherwise this is a security breach (writing to arbitrary user files...).
 * Constraints (validated by validate()):
 * - <payload_root> is a simple dir_name (no ".." or "/").
 * - any <file_relative_path> must be relative and have no "..".
 *
 * The sender will user next_chunk_size () and send_next_chunk () until there are no more.
 * Call receive chunk with chunk size until total_transfered==total_size.
 *
 * Chunks are not cut by file boundaries: they operate on the concantenated data of all files.
 * Multiple files may be sent in one chunk; data is dispatched according to file limits.
 * When a file has been completely sent, its checksum is available and can be sent.
 * Upload is complete if all data then checksums have been sent.
 * Download is complete if all data then checksums have been received (and checkums valid).
 *
 * Note: This class never checks the status of the stream object.
 *
 * TODO ability to set a position (for restarts) ?
 */
class Manager : public Streamable {
	Q_DECLARE_TR_FUNCTIONS (Manager);

public:
	enum Mode { Closed, Sending, Receiving };
	enum PayloadType { Invalid, SingleFile, Directory };
	using Checksum = QByteArray;
	using ChecksumList = QList<Checksum>;

private:
	using FileList = std::list<File>; // std::list can handle File (non copyable/movable)

	QString last_error;

	// Transfer display information
	qint64 total_size{0};
	// transfer name and fullpath are recomputed

	QDir root_dir;        // Should always store an absolute path
	QString payload_root; // '.' for SingleFile, '<dir>' for Directory
	FileList files;

	// Progress
	Mode transfer_status{Closed};
	FileList::iterator current_file{files.end ()};
	FileList::iterator next_file_to_checksum{files.end ()};
	qint64 total_transfered{0};
	int nb_files_transfered{0};

public:
	QString get_last_error (void) const { return last_error; }

	qint64 get_total_size (void) const { return total_size; }
	qint64 get_total_transfered_size (void) const { return total_transfered; }
	int get_nb_files (void) const { return int(files.size ()); }
	int get_nb_files_transfered (void) const { return nb_files_transfered; }

	const QDir & get_root_dir (void) const { return root_dir; }
	void set_root_dir (const QString & dir_path) {
		Q_ASSERT (transfer_status == Closed);
		root_dir.setPath (dir_path);
	}

	PayloadType get_type (void) const {
		if (payload_root.isEmpty ())
			return Invalid;
		else if (payload_root == ".")
			return SingleFile;
		else
			return Directory;
	}
	QString get_payload_name (void) const {
		switch (get_type ()) {
		case SingleFile:
			return files.front ().get_relative_path ();
		case Directory:
			return payload_root + QDir::separator ();
		default:
			return QString ();
		}
	}
	QString get_payload_dir_display (void) const {
		switch (get_type ()) {
		case SingleFile:
			return QDir::toNativeSeparators (root_dir.filePath (files.front ().get_relative_path ()));
		case Directory:
			return QDir::toNativeSeparators (get_payload_dir ().path ()) + QDir::separator ();
		default:
			return QString ();
		}
	}
	QString inspect_files (void) const {
		QString text;
		for (auto & f : files)
			text += QStringLiteral ("-\t%1 (%2)\n")
			            .arg (f.get_relative_path (), size_to_string (f.get_size ()));
		return text;
	}

	// File list management

	bool from_source_path (const QString & path, bool ignore_hidden) {
		Q_ASSERT (transfer_status == Closed);
		Q_ASSERT (get_type () == Invalid); // Should only be called once
		auto cleaned_path = QFileInfo (path).canonicalFilePath ();
		if (cleaned_path.isEmpty ()) {
			last_error = tr ("Invalid path: %1").arg (path);
			return false;
		}
		QFileInfo path_info (cleaned_path);
		root_dir = path_info.dir ();
		if (path_info.isFile ()) {
			payload_root = ".";
			files.emplace_back (path_info, root_dir);
			total_size += path_info.size ();
			return true;
		} else if (path_info.isDir ()) {
			payload_root = path_info.fileName ();
			auto payload_dir = get_payload_dir ();
			// Recursively search dirs for files
			auto filter_flags = QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Readable;
			if (!ignore_hidden)
				filter_flags |= QDir::Hidden;
			QDirIterator it (path_info.filePath (), filter_flags, QDirIterator::Subdirectories);
			QElapsedTimer timer;
			timer.start();
			while (it.hasNext ()) {
				if (timer.elapsed() > Const::max_work_msec) {
					// Let event loop run (warning! may cause data races)
					QCoreApplication::processEvents ();
					timer.start ();
				}
				QFileInfo entry (it.next ());
				files.emplace_back (entry, payload_dir);
				total_size += entry.size ();
			}
			if (files.empty ()) {
				last_error = tr ("No file found in directory: %1").arg (path);
				return false;
			}
			return true;
		} else {
			last_error = tr ("Path is neither a file nor a directory: %1").arg (path);
			return false;
		}
	}

	// Import/export. File class is not movable nor copyable, so extra care is needed.

	void to_stream (QDataStream & stream) const {
		Q_ASSERT (get_type () != Invalid);
		stream << payload_root << total_size << quint32 (files.size ());
		for (const auto & f : files)
			stream << f;
	}
	void from_stream (QDataStream & stream) {
		Q_ASSERT (transfer_status == Closed);
		Q_ASSERT (get_type () == Invalid); // Should only be called once
		quint32 c;
		stream >> payload_root >> total_size >> c;
		files.clear ();
		for (quint32 i = 0; i < c; ++i) {
			files.emplace_back ();
			stream >> files.back ();
		}
	}
	bool validate (void) const {
		if (total_size < 0)
			return false;
		if (payload_root.contains ("..") || payload_root.contains ('/') || payload_root.contains ('\\'))
			return false;
		if (files.empty ())
			return false;
		for (auto & f : files)
			if (!f.validate_path ())
				return false;
		return true;
	}

	// Transfer status (open/close like)

	void start_transfer (Mode mode) {
		Q_ASSERT (get_type () != Invalid);
		Q_ASSERT (transfer_status == Closed);
		Q_ASSERT (mode != Closed);
		transfer_status = mode;
		total_transfered = 0;
		nb_files_transfered = 0;
		current_file = next_file_to_checksum = files.begin ();
	}

	void stop_transfer (void) {
		if (current_file != files.end ())
			current_file->close ();
		current_file = next_file_to_checksum = files.end ();
		transfer_status = Closed;
	}

	bool is_transfer_complete (void) const {
		// No specific marker for success, but this should catch all cases.
		return transfer_status == Closed && last_error.isEmpty () && total_transfered == total_size;
	}

	// Send / receive next chunk

	qint64 next_chunk_size (void) const {
		// Chunk are all of size Const::chunk_size, except the last which is truncated
		// 0 means no more to transfer
		Q_ASSERT (total_transfered <= total_size);
		return qMin (Const::chunk_size, total_size - total_transfered);
	}

	bool send_next_chunk (QDataStream & stream) {
		Q_ASSERT (transfer_status == Sending);
		auto bytes_to_send = next_chunk_size ();
		while (bytes_to_send > 0) {
			Q_ASSERT (total_transfered <= total_size);
			Q_ASSERT (nb_files_transfered <= get_nb_files ());
			Q_ASSERT (current_file != files.end ()); // Should stop due to size test
			if (!current_file->is_open () &&
			    !current_file->open (get_payload_dir (), QIODevice::ReadOnly)) {
				transfer_error (current_file->get_last_error ());
				return false;
			}
			auto sent = current_file->read_data (stream, bytes_to_send);
			if (sent == -1) {
				transfer_error (
				    tr ("Unable to send data to socket: %1").arg (stream.device ()->errorString ()));
				return false;
			}
			bytes_to_send -= sent;
			total_transfered += sent;
			if (current_file->at_end ()) {
				current_file->close ();
				current_file++;
			}
		}
		if (total_transfered == total_size)
			Q_ASSERT (current_file == files.end ());
		return true;
	}

	bool receive_chunk (QDataStream & stream, qint64 chunk_size) {
		Q_ASSERT (transfer_status == Receiving);
		if (chunk_size > (total_size - total_transfered)) {
			transfer_error (tr ("Chunk goes past the end of transfer"));
			return false;
		}
		auto bytes_to_receive = chunk_size;
		while (bytes_to_receive > 0) {
			Q_ASSERT (total_transfered <= total_size);
			Q_ASSERT (current_file != files.end ()); // Should stop due to size test
			if (!current_file->is_open () &&
			    !current_file->open (get_payload_dir (), QIODevice::ReadWrite)) {
				transfer_error (current_file->get_last_error ());
				return false;
			}
			auto received = current_file->write_data (stream, bytes_to_receive);
			if (received == -1) {
				transfer_error (
				    tr ("Unable to receive data from socket: %1").arg (stream.device ()->errorString ()));
				return false;
			}
			bytes_to_receive -= received;
			total_transfered += received;
			if (current_file->at_end ()) {
				current_file->close ();
				current_file++;
			}
		}
		if (total_transfered == total_size)
			Q_ASSERT (current_file == files.end ());
		return true;
	}

	// Checksums

	ChecksumList take_pending_checksums (void) {
		ChecksumList checksums;
		// We can only send checksums if files have been processed
		for (; next_file_to_checksum != current_file; ++next_file_to_checksum) {
			checksums.append (next_file_to_checksum->get_checksum ());
			++nb_files_transfered;
		}
		if (next_file_to_checksum == files.end ()) {
			Q_ASSERT (nb_files_transfered == get_nb_files ());
			Q_ASSERT (total_transfered == total_size);
			stop_transfer (); // Close the transfer
		}
		return checksums;
	}

	bool test_checksums (const ChecksumList & checksums) {
		// Test checksums against files (must have been processed before)
		for (const auto & checksum : checksums) {
			if (next_file_to_checksum == current_file) {
				transfer_error (tr ("Received checksum of incomplete file."));
				return false;
			}
			if (!next_file_to_checksum->test_checksum (checksum)) {
				last_error = next_file_to_checksum->get_last_error ();
				return false;
			}
			++next_file_to_checksum;
			++nb_files_transfered;
		}
		if (next_file_to_checksum == files.end ()) {
			Q_ASSERT (nb_files_transfered == get_nb_files ());
			Q_ASSERT (total_transfered == total_size);
			stop_transfer (); // Close the transfer
		}
		return true;
	}

private:
	QDir get_payload_dir (void) const { return QDir (root_dir.filePath (payload_root)); }

	void transfer_error (const QString & why) {
		last_error = why;
		stop_transfer ();
	}
};
}

#endif
