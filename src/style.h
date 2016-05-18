#ifndef STYLE_H
#define STYLE_H

#include <QIcon>
#include <QStyle>
#include <QApplication>

namespace Icon {
// helper
inline QIcon from_style (QStyle::StandardPixmap icon) {
	return QApplication::style ()->standardIcon (icon);
}

// High def for mac
inline QIcon app (void) {
	return QIcon (":/icon.svg");
}

// Appear in toolbar
inline QIcon send (void) {
	return QIcon (":/send_file.svg");
}
inline QIcon add_peer (void) {
	return QIcon (":/peer_add.svg");
}

// Optional
inline QIcon restore (void) {
	return QIcon::fromTheme ("view-restore");
}
inline QIcon quit (void) {
	return QIcon::fromTheme ("application-exit");
}

// Matching pair
inline QIcon download (void) {
	return QIcon (":/download.svg");
}
inline QIcon upload (void) {
	return QIcon (":/upload.svg");
}

// View buttons
inline QIcon accept (void) {
	return from_style (QStyle::SP_DialogOkButton);
}
inline QIcon cancel (void) {
	return from_style (QStyle::SP_DialogCancelButton);
}
inline QIcon change_download_path (void) {
	return QIcon::fromTheme ("emblem-downloads", from_style (QStyle::SP_DirIcon));
}
inline QIcon delete_transfer (void) {
	return QIcon::fromTheme ("edit-delete", from_style (QStyle::SP_TrashIcon));
}
inline QIcon delete_peer (void) {
	return QIcon (":/peer_remove.svg");
}
}

#endif
