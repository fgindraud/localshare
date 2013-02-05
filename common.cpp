#include "localshare.h"
#include "common.h"

#include <QMessageBox>
#include <QProcessEnvironment>
#include <QSystemTrayIcon>
#include <QApplication>
#include <QDir>

/* ----- Settings ----- */

QSettings Settings::settings (APP_NAME, APP_NAME);

static const QString networkNameKey ("network/name");
static const QString networkTcpPortKey ("network/tcpPort");
static const QString downloadPathKey ("download/path");
static const QString alwaysDownladKey ("download/alwaysAccept");
static const QString useSystemTrayKey ("gui/useSystemTray");

/*
 * network/name
 * Peer name on the network
 */

static void namePostProcessor (QString & str) { str.truncate (NAME_SIZE_LIMIT); }

QString Settings::defaultName (void) {
	QStringList candidates;
	// Try to get a username from classical env variables
	candidates << "USER" << "USERNAME";
	candidates << "HOSTNAME";

	QProcessEnvironment env = QProcessEnvironment::systemEnvironment ();
	foreach (QString key, candidates)
		if (env.contains (key))
			return env.value (key);

	// Or return default if not found
	return "Unknown";
}

QString Settings::name (void) {
	return getValueCached (networkNameKey, defaultName, namePostProcessor);
}

void Settings::setName (QString & name) { settings.setValue (networkNameKey, name); }

/*
 * network/tcpPort
 * Opened tcp port (default is in localshare.h)
 */
quint16 Settings::defaultTcpPort (void) { return DEFAULT_TCP_PORT; }

quint16 Settings::tcpPort (void) {
	return getValueCached (networkTcpPortKey, defaultTcpPort);
}

void Settings::setTcpPort (quint16 port) {
	settings.setValue (networkTcpPortKey, port);
}

/*
 * download/path
 * Path to where files are stored (default = homepath)
 */
QString Settings::defaultDownloadPath (void) { return QDir::homePath (); }

QString Settings::downloadPath (void) {
	return getValueCached (downloadPathKey, defaultDownloadPath);
}

void Settings::setDownloadPath (const QString & path) {
	settings.setValue (downloadPathKey, path);
}

/*
 * download/alwaysAccept
 * Automatically accepts all file proposals (defaults to false)
 */
bool Settings::defaultAlwaysDownload (void) { return false; }

bool Settings::alwaysDownload (void) {
	return getValueCached (alwaysDownladKey, defaultAlwaysDownload);
}

void Settings::setAlwaysDownload (bool always) {
	settings.setValue (alwaysDownladKey, always);
}

/*
 * gui/useSystemTray
 */
static void useSystemTrayPostProcessor (bool & enabled) {
	enabled = enabled && QSystemTrayIcon::isSystemTrayAvailable ();
}

bool Settings::defaultUseSystemTray (void) { return true; }

bool Settings::useSystemTray (void) {
	return getValueCached (useSystemTrayKey, defaultUseSystemTray, useSystemTrayPostProcessor);
}

void Settings::setUseSystemTray (bool enabled) {
	settings.setValue (useSystemTrayKey, enabled);
}

/* ----- Message ----- */

/*
 * User-level error and warning messages
 */

void Message::error (const QString & title, const QString & message) {
	QMessageBox::critical (0, title, message);
	qApp->quit ();
}

void Message::warning (const QString & title, const QString & message) {
	QMessageBox::warning (0, title, message);
}

/* ------ Icons ------ */

QIcon Icon::app (void) { return QIcon (":/icon.svg"); }

QIcon Icon::file (void) { return style.standardIcon (QStyle::SP_FileIcon); }

QIcon Icon::openFile (void) { return style.standardIcon (QStyle::SP_DirIcon); }
QIcon Icon::settings (void) { return style.standardIcon (QStyle::SP_ComputerIcon); }

QIcon Icon::accept (void) { return style.standardIcon (QStyle::SP_DialogOkButton); }
QIcon Icon::closeAbort (void) { return style.standardIcon (QStyle::SP_DialogCancelButton); }

QIcon Icon::inbound (void) { return style.standardIcon (QStyle::SP_ArrowDown); }
QIcon Icon::outbound (void) { return style.standardIcon (QStyle::SP_ArrowUp); }

QCommonStyle Icon::style;

/* ------ File size ------ */

QString fileSizeToString (quint64 size) {
	double num = size;
	QStringList list;

	double increment = 1024.0;
	list << "kio" << "Mio" << "Gio" << "Tio";

	QStringListIterator i (list);
	QString unit ("o");

	while (num >= increment && i.hasNext ()) {
		unit = i.next ();
		num /= increment;
	}
	return QString ().setNum (num, 'f', 2) + unit;
}

