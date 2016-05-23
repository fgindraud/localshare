#pragma once
#ifndef CONSOLE_MAIN_H
#define CONSOLE_MAIN_H

#include <QByteArray>
#include <QCoreApplication>

#include "core/localshare.h"

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
	qDebug ("Batch mode, TODO");


/*	
	QFile file ("/home/fgindraud/todo");
	qDebug () << file.exists ();
	qDebug () << file.open (QIODevice::ReadOnly);
	auto p = file.map (0, file.size ());
	qDebug () << p;
	QTimer timer;
	timer.connect (&timer, &QTimer::timeout, [&] { qDebug () << file.size (); });
	timer.start (1000);
*/

	return app.exec ();
}

#endif
