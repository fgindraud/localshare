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
#include <QApplication>

#include "core_localshare.h"
#include "gui_main.h"
#include "gui_style.h"
#include "gui_window.h"

namespace Gui {
int start (int & argc, char **& argv) {
	QApplication app (argc, argv);
	Const::setup (app);

	// Set icons, start app
	app.setWindowIcon (Icon::app ());
	Window window;
	return app.exec ();
}
}
