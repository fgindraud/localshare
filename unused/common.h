#ifndef H_COMMON
#define H_COMMON

#include <QString>
#include <QHostAddress>
#include <QCommonStyle>
#include <QIcon>
#include <QSettings>

/*
 * Handle program settings, and defaults values
 */
namespace Settings {
/* Network settings */
QString name (void);
void setName (const QString & name);

quint16 tcpPort (void);
void setTcpPort (quint16 port);

int tcpKeepAliveTime (void);
void setTcpKeepAliveTime (int sec);

/* Download path/confirmation settings */
QString downloadPath (void);
void setDownloadPath (const QString & path);

bool alwaysDownload (void);
bool defaultAlwaysDownload (void);
void setAlwaysDownload (bool always);

/* Use system tray */
bool useSystemTray (void);
void setUseSystemTray (bool use);
}

/*
 * Error messages
 */
namespace Message {
void error (const QString & title, const QString & message);
void warning (const QString & title, const QString & message);
}

/*
 * Icon settings for the application
 */
namespace Icon {
// Application icon
QIcon app (void);

// Represents a file
QIcon file (void);

// Open file & Settings dialog buttons
QIcon openFile (void);
QIcon settings (void);

// Accept and refuse/close icon
QIcon accept (void);
QIcon closeAbort (void);

// In/out-bound transfer icons
QIcon inbound (void);
QIcon outbound (void);
};

/*
 * File utilities
 */
namespace FileUtils {
using Size = quint64;

/*
 * Give a human readable string for file size.
 */
QString sizeToString (Size size);

/*
 * Check if a file is readable, and get its size and basename
 */
bool infoCheck (const QString filename, Size * size = nullptr, QString * baseName = nullptr);
};

#endif
