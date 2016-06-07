#pragma once
#ifndef GUI_STYLE_H
#define GUI_STYLE_H

#include <QApplication>
#include <QIcon>
#include <QStyle>

namespace Icon {
// helper
inline QIcon from_style (QStyle::StandardPixmap icon) {
	return QApplication::style ()->standardIcon (icon);
}

// High def for mac
inline QIcon app (void) {
	return QIcon (QStringLiteral (":/icon.svg"));
}

// Warning
inline QIcon warning (void) {
	return from_style (QStyle::SP_MessageBoxWarning);
}

// Appear in toolbar size
inline QIcon send_file (void) {
	return QIcon (QStringLiteral (":/send_file.svg"));
}
inline QIcon send_dir (void) {
	return QIcon (QStringLiteral (":/send_directory.svg"));
}
inline QIcon add_peer (void) {
	return QIcon (QStringLiteral (":/peer_add.svg"));
}
inline QIcon restart_discovery (void) {
	return QIcon (QStringLiteral (":/restart_discovery.svg"));
}

// Optional
inline QIcon restore (void) {
	return QIcon::fromTheme (QStringLiteral ("view-restore"));
}
inline QIcon quit (void) {
	return QIcon::fromTheme (QStringLiteral ("application-exit"));
}

// Matching pair
inline QIcon download (void) {
	return QIcon (QStringLiteral (":/download.svg"));
}
inline QIcon upload (void) {
	return QIcon (QStringLiteral (":/upload.svg"));
}

// Options
inline QIcon change_username (void) {
	return QIcon (QStringLiteral (":/change_username.svg"));
}
inline QIcon download_auto (void) {
	return QIcon (QStringLiteral (":/download_auto.svg"));
}

// View buttons
inline QIcon accept (void) {
	return from_style (QStyle::SP_DialogOkButton);
}
inline QIcon cancel (void) {
	return from_style (QStyle::SP_DialogCancelButton);
}
inline QIcon change_download_path (void) {
	// Also used for option
	return QIcon::fromTheme (QStringLiteral ("emblem-downloads"),
	                         QIcon (QStringLiteral (":/download_path.svg")));
}
inline QIcon delete_transfer (void) {
	return QIcon::fromTheme (QStringLiteral ("edit-delete"), from_style (QStyle::SP_TrashIcon));
}
inline QIcon delete_peer (void) {
	return QIcon (QStringLiteral (":/peer_remove.svg"));
}
}

#endif
