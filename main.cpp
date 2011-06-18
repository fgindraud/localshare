#include "trayIcon.h"
#include "common.h"

#include <QApplication>

int main (int argc, char *argv[]) {
	QApplication app (argc, argv);
	app.setQuitOnLastWindowClosed (false);
	
	TrayIcon trayIcon;
	Settings settings;

	return app.exec ();
}

