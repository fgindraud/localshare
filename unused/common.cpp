#include "localshare.h"
#include "common.h"

#include <utility>

#include <QMessageBox>
#include <QProcessEnvironment>
#include <QSystemTrayIcon>
#include <QApplication>
#include <QDir>
#include <QFileInfo>

	    [](bool enabled) { return enabled && QSystemTrayIcon::isSystemTrayAvailable (); });

/* ----- Message ----- */

/*
 * User-level error and warning messages
 */

namespace Message {
void error (const QString & title, const QString & message) {
	QMessageBox::critical (0, title, message);
	qApp->quit ();
}

void warning (const QString & title, const QString & message) {
	QMessageBox::warning (0, title, message);
}
}

/* ------ Icons ------ */

namespace Icon {
namespace {
	QCommonStyle style;
}

QIcon app (void) {
	return QIcon (":/icon.svg");
}

QIcon file (void) {
	return style.standardIcon (QStyle::SP_FileIcon);
}

QIcon openFile (void) {
	return style.standardIcon (QStyle::SP_DirIcon);
}
QIcon settings (void) {
	return style.standardIcon (QStyle::SP_ComputerIcon);
}

QIcon accept (void) {
	return style.standardIcon (QStyle::SP_DialogOkButton);
}
QIcon closeAbort (void) {
	return style.standardIcon (QStyle::SP_DialogCancelButton);
}

QIcon inbound (void) {
	return style.standardIcon (QStyle::SP_ArrowDown);
}
QIcon outbound (void) {
	return style.standardIcon (QStyle::SP_ArrowUp);
}
}

/* ------ File size ------ */

namespace FileUtils {
QString sizeToString (Size size) {
	double num = size;
	double increment = 1024.0;
	const char * suffixes[] = {"o", "kio", "Mio", "Gio", "Tio", nullptr};
	int unit_idx = 0;
	while (num >= increment && suffixes[unit_idx + 1] != nullptr) {
		unit_idx++;
		num /= increment;
	}
	return QString ().setNum (num, 'f', 2) + suffixes[unit_idx];
}

bool infoCheck (const QString filename, Size * size, QString * baseName) {
	QFileInfo fileInfo (filename);
	if (fileInfo.exists () && fileInfo.isFile () && fileInfo.isReadable ()) {
		if (size != 0)
			*size = fileInfo.size ();
		if (baseName != 0)
			*baseName = fileInfo.fileName ();
		return true;
	} else {
		return false;
	}
}
}
