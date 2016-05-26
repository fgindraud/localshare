#pragma once
#ifndef CONSOLE_MAIN_H
#define CONSOLE_MAIN_H

#include <QByteArray>
#include <QCoreApplication>

#include <QIODevice>
#include <cstring>

#include "core/localshare.h"
#include "core/payload.h"

/* TODO batch mode
 * start a QCoreApplication instead in batch mode
 * we can be similar to netcat:
 * -d to set download mode
 * -u to set upload mode
 * -p to filter/set peer name
 * -y to auto accept
 *  etc...
 */

inline bool is_console_mode (int argc, const char * const * argv) {
	for (int i = 1; i < argc; ++i)
		if (qstrcmp ("-d", argv[i]) == 0 || qstrcmp ("-u", argv[i]) == 0)
			return true;
	return false;
}

inline int console_main (int & argc, char **& argv) {
	QCoreApplication app (argc, argv);
	Const::setup (app);
	qDebug ("Batch mode, TODO, currently test area");

#if 1
	if (argc <= 3) {
		qDebug () << "usage"
		          << "prog"
		          << "source"
		          << "target";
		return 0;
	}

	Payload::Manager payload;
	payload.from_source_path (argv[1], true);
	qDebug () << "payload" << payload;

	Payload::Manager target;
	target.set_root_dir (argv[2]);

	QByteArray buf;
	{
		QDataStream s (&buf, QIODevice::WriteOnly);
		s << payload;
		payload.start_transfer (Payload::Manager::Sending);
		while (payload.next_chunk_size () > 0) {
			if (!payload.send_next_chunk (s)) {
				qDebug () << "fail send_next_chunk" << payload.get_last_error ();
				return 0;
			}
		}
		s << payload.take_pending_checksums ();
	}
	{
		QDataStream s (&buf, QIODevice::ReadOnly);
		s >> target;
		qDebug () << "target" << target;
		target.start_transfer (Payload::Manager::Receiving);
		if (!target.receive_chunk (s, target.get_total_size ())) {
			qDebug () << "fail receive_chunk" << target.get_last_error ();
			return 0;
		}
		Payload::Manager::ChecksumList csl;
		s >> csl;
		if (!target.test_checksums (csl)) {
			qDebug () << "fail test_checksums" << target.get_last_error ();
			return 0;
		}
	}
#endif
	return 0;
	return app.exec ();
}

#endif
