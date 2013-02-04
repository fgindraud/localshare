#include "localshare.h"
#include "common.h"

#include <QMessageBox>
#include <QProcessEnvironment>

/* ----- Settings ----- */

Settings::Settings () : QSettings (APP_NAME, APP_NAME) {
}

/*
 * network/name
 * Peer name on the network
 */
static const QString networkNameKey ("network/name");

QString Settings::name (void) const {
	if (contains (networkNameKey)) {
		// Get it from config
		QString tmp = value (networkNameKey).toString ();
		tmp.truncate (NAME_SIZE_LIMIT);
		return tmp;
	} else {
		QStringList candidates;
		// Try to get username from classical env variables
		candidates << "USER" << "USERNAME";
		candidates << "HOSTNAME";
		
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment ();
		for (QStringList::const_iterator it = candidates.constBegin (); 
			it != candidates.constEnd (); ++it)
			if (env.contains (*it)) {
				QString name = env.value (*it);
				name.truncate (NAME_SIZE_LIMIT);
				return name;
			}

		// Or return default if not found
		return "Unknown";
	}
}

void Settings::setName (QString & name) {
	name.truncate (NAME_SIZE_LIMIT);
	setValue (networkNameKey, name);
}

/*
 * network/tcpPort
 * Opened tcp port (default is in localshare.h)
 */
static const QString networkTcpPortKey ("network/tcpPort");

quint16 Settings::tcpPort (void) const {
	return static_cast<quint16> (value (networkTcpPortKey, DEFAULT_TCP_PORT).toUInt ());
}

void Settings::setTcpPort (quint16 port) {
	setValue (networkTcpPortKey, static_cast<uint> (port));
}

/*
 * download/path
 * Path to where files are stored (default = homepath)
 */
static const QString downloadPathKey ("download/path");

QString Settings::downloadPath (void) const {
	return value (downloadPathKey, QDir::homePath ()).toString ();
}

void Settings::setDownloadPath (const QString & path) {
	setValue (downloadPathKey, path);
}

/*
 * download/alwaysAccept
 * Automatically accepts all file proposals (defaults to false)
 */
static const QString alwaysDownladKey ("download/alwaysAccept");

bool Settings::alwaysDownload (void) const {
	return value (alwaysDownladKey, false).toBool ();
}

void Settings::setAlwaysDownload (bool always) {
	setValue (alwaysDownladKey, always);
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

IconFactory::IconFactory () { style = new QCommonStyle; } 
IconFactory::~IconFactory () { delete style; }

QIcon IconFactory::appIcon (void) {
	return QIcon (":/icon.svg");
}

QIcon IconFactory::fileIcon (void) {
	return style->standardIcon (QStyle::SP_DirIcon);
}
QIcon IconFactory::settingsIcon (void) {
	return style->standardIcon (QStyle::SP_ComputerIcon);
}

QIcon IconFactory::acceptIcon (void) {
	return style->standardIcon (QStyle::SP_DialogOkButton);
}
QIcon IconFactory::closeAbortIcon (void) {
	return style->standardIcon (QStyle::SP_DialogCancelButton);
}

QIcon IconFactory::inboundIcon (void) {
	return style->standardIcon (QStyle::SP_ArrowDown);
}
QIcon IconFactory::outboundIcon (void) {
	return style->standardIcon (QStyle::SP_ArrowUp);
}

IconFactory appIcons;

