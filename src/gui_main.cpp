#include <QApplication>

#include "core_localshare.h"
#include "gui_style.h"
#include "gui_window.h"
#include "gui_main.h"

int gui_main (int & argc, char **& argv) {
	QApplication app (argc, argv);
	Const::setup (app);

	// Set icons, start app
	app.setWindowIcon (Icon::app ());
	Window window;
	return app.exec ();
}
