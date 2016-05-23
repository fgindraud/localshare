#pragma once
#ifndef GUI_MAIN_H
#define GUI_MAIN_H

#include <QApplication>

#include "core/localshare.h"
#include "gui/style.h"
#include "gui/window.h"

inline int gui_main (int & argc, char **& argv) {
	QApplication app (argc, argv);
	Const::setup (app);

	// Set icons, start app
	app.setWindowIcon (Icon::app ());
	Window window;
	return app.exec ();
}

#endif
