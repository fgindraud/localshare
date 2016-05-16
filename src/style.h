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

inline QIcon app (void) {
	return QIcon (":/icon.svg");
}

inline QIcon send (void) {
	return from_style (QStyle::SP_FileIcon);
}
inline QIcon restore (void) {
	return QIcon::fromTheme ("view-restore");
}
inline QIcon quit (void) {
	return QIcon::fromTheme ("application-exit", from_style (QStyle::SP_DialogCloseButton));
}

inline QIcon download (void) {
	return from_style (QStyle::SP_ArrowDown);
}
inline QIcon upload (void) {
	return from_style (QStyle::SP_ArrowUp);
}

inline QIcon accept (void) {
	return from_style (QStyle::SP_DialogOkButton);
}
inline QIcon cancel (void) {
	return from_style (QStyle::SP_DialogCancelButton);
}
inline QIcon change_download_path (void) {
	return from_style (QStyle::SP_DirIcon);
}
}

#endif
