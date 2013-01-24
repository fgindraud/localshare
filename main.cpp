#include "trayIcon.h"
#include "common.h"

#include <QApplication>

int main (int argc, char *argv[]) {
	QApplication app (argc, argv);

	// Using no main window, so disable stupid defaults
	app.setQuitOnLastWindowClosed (false);

	// Icon
	app.setWindowIcon (QIcon ("icon.svg"));

	// Create tray icon, and show it.
	TrayIcon trayIcon;

	return app.exec ();
}

