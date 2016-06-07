/* Localshare - Small file sharing application for the local network.
 * Copyright (C) 2016 Francois Gindraud
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef GUI_STYLE_H
#define GUI_STYLE_H

#include <QApplication>
#include <QIcon>
#include <QStyle>

namespace Gui {
namespace Icon {
	// helper
	inline QIcon from_style (QStyle::StandardPixmap icon) {
		return QApplication::style ()->standardIcon (icon);
	}

	// High def for mac
	inline QIcon app (void) { return QIcon (QStringLiteral (":/icon.svg")); }

	// Warning
	inline QIcon warning (void) { return from_style (QStyle::SP_MessageBoxWarning); }

	// Appear in toolbar size
	inline QIcon send_file (void) { return QIcon (QStringLiteral (":/send_file.svg")); }
	inline QIcon send_dir (void) { return QIcon (QStringLiteral (":/send_directory.svg")); }
	inline QIcon add_peer (void) { return QIcon (QStringLiteral (":/peer_add.svg")); }
	inline QIcon restart_discovery (void) {
		return QIcon (QStringLiteral (":/restart_discovery.svg"));
	}

	// Optional
	inline QIcon restore (void) { return QIcon::fromTheme (QStringLiteral ("view-restore")); }
	inline QIcon quit (void) { return QIcon::fromTheme (QStringLiteral ("application-exit")); }

	// Matching pair
	inline QIcon download (void) { return QIcon (QStringLiteral (":/download.svg")); }
	inline QIcon upload (void) { return QIcon (QStringLiteral (":/upload.svg")); }

	// Options
	inline QIcon change_username (void) { return QIcon (QStringLiteral (":/change_username.svg")); }
	inline QIcon download_auto (void) { return QIcon (QStringLiteral (":/download_auto.svg")); }
	inline QIcon system_tray (void) { return QIcon (QStringLiteral (":/system_tray.svg")); }
	inline QIcon hidden_files (void) { return QIcon (QStringLiteral (":/hidden_files.svg")); }

	// View buttons
	inline QIcon accept (void) { return from_style (QStyle::SP_DialogOkButton); }
	inline QIcon cancel (void) { return from_style (QStyle::SP_DialogCancelButton); }
	inline QIcon change_download_path (void) {
		// Also used for option
		return QIcon::fromTheme (QStringLiteral ("emblem-downloads"),
		                         QIcon (QStringLiteral (":/download_path.svg")));
	}
	inline QIcon delete_transfer (void) {
		return QIcon::fromTheme (QStringLiteral ("edit-delete"), from_style (QStyle::SP_TrashIcon));
	}
	inline QIcon delete_peer (void) { return QIcon (QStringLiteral (":/peer_remove.svg")); }
}
}

#endif
