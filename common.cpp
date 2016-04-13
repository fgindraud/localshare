#include "localshare.h"
#include "common.h"

#include <utility>

#include <QMessageBox>
#include <QProcessEnvironment>
#include <QSystemTrayIcon>
#include <QApplication>
#include <QDir>
#include <QFileInfo>

/* ----- Settings ----- */

namespace Settings {
namespace {
	QSettings settings (APP_NAME, APP_NAME);

	template <typename T, typename DefaultFactory, typename Normalizer>
	T getValueCached (const QString & key, DefaultFactory && defaultFactory,
	                  Normalizer && normalizer) {
		// Cache value if not already done
		if (not settings.contains (key))
			settings.setValue (key, normalizer (defaultFactory ()));
		// Get value
		return normalizer (settings.value (key).value<T> ());
	}
	
	template <typename T, typename DefaultFactory>
	T getValueCached (const QString & key, DefaultFactory && defaultFactory) {
		return getValueCached<T> (key, std::forward<DefaultFactory> (defaultFactory), [] (T arg) { return arg; });
	}

	const QString networkNameKey ("network/name");
	const QString networkTcpPortKey ("network/tcpPort");
	const QString networkTcpKeepAliveTimeKey ("network/tcpKeepAliveTime");

	const QString downloadPathKey ("download/path");
	const QString alwaysDownloadKey ("download/alwaysAccept");

	const QString useSystemTrayKey ("gui/useSystemTray");
}

/*
 * network/name
 * Peer name on the network
 */
QString name (void) {
	return getValueCached<QString> (
	    networkNameKey,
	    [] {
		    // Try to get a username from classical env variables
		    const char * candidates[] = {"USER", "USERNAME", "HOSTNAME", "LOGNAME"};
		    QProcessEnvironment env = QProcessEnvironment::systemEnvironment ();
		    for (auto key : candidates)
			    if (env.contains (key))
				    return env.value (key);

		    // Or return default if not found
		    return QString ("Unknown");
		  },
	    [](QString && str) {
		    str.truncate (NAME_SIZE_LIMIT);
		    return str;
		  });
}

void setName (const QString & name) {
	settings.setValue (networkNameKey, name);
}

/*
 * network/tcpPort
 * Opened tcp port (default is in localshare.h)
 */
quint16 tcpPort (void) {
	return getValueCached<quint16> (networkTcpPortKey, [] { return DEFAULT_TCP_PORT; });
}

void setTcpPort (quint16 port) {
	settings.setValue (networkTcpPortKey, port);
}

/*
 * network/tcpKeepAliveTime
 * Time in seconds before closing a connection between two peers when it is unused.
 */
int tcpKeepAliveTime (void) {
	return getValueCached<int> (networkTcpKeepAliveTimeKey, [] { return DEFAULT_KEEP_ALIVE; },
	                            [](int sec) {
		                            if (sec < 0)
			                            return 0;
		                            else
			                            return sec;
		                          });
}

void setTcpKeepAliveTime (int sec) {
	settings.setValue (networkTcpKeepAliveTimeKey, sec);
}

/*
 * download/path
 * Path to where files are stored (default = homepath)
 */
QString downloadPath (void) {
	return getValueCached<QString> (downloadPathKey, [] { return QDir::homePath (); });
}

void setDownloadPath (const QString & path) {
	settings.setValue (downloadPathKey, path);
}

/*
 * download/alwaysAccept
 * Automatically accepts all file proposals (defaults to false)
 */
bool alwaysDownload (void) {
	return getValueCached<bool> (alwaysDownloadKey, [] { return false; });
}

void setAlwaysDownload (bool always) {
	settings.setValue (alwaysDownloadKey, always);
}

/*
 * gui/useSystemTray
 */
bool useSystemTray (void) {
	return getValueCached<bool> (
	    useSystemTrayKey, [] { return true; },
	    [](bool enabled) { return enabled && QSystemTrayIcon::isSystemTrayAvailable (); });
}

void setUseSystemTray (bool enabled) {
	settings.setValue (useSystemTrayKey, enabled);
}
}

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
