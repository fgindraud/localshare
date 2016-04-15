#ifndef STYLE_H
#define STYLE_H

#include <QIcon>
#include <QStyle>
#include <QApplication>

namespace Icon {
// helper
inline QIcon from_style (QStyle::StandardPixmap icon) {
	return qApp->style ()->standardIcon (icon);
}

inline QIcon app (void) {
	return QIcon (":/icon.svg");
}

inline QIcon file (void) {
	return from_style (QStyle::SP_FileIcon);
}
inline QIcon openFile (void) {
	return from_style (QStyle::SP_DirIcon);
}
inline QIcon settings (void) {
	return from_style (QStyle::SP_ComputerIcon);
}
inline QIcon accept (void) {
	return from_style (QStyle::SP_DialogOkButton);
}
inline QIcon closeAbort (void) {
	return from_style (QStyle::SP_DialogCancelButton);
}
inline QIcon inbound (void) {
	return from_style (QStyle::SP_ArrowDown);
}
inline QIcon outbound (void) {
	return from_style (QStyle::SP_ArrowUp);
}
}

#endif
