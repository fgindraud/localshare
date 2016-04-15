#ifndef H_COMMON
#define H_COMMON

#include <QString>
#include <QHostAddress>
#include <QCommonStyle>
#include <QIcon>
#include <QSettings>

/*
 * Error messages
 */
namespace Message {
void error (const QString & title, const QString & message);
void warning (const QString & title, const QString & message);
}

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
