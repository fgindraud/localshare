#include "trayIcon.h"
#include "common.h"

#include <QApplication>

int main (int argc, char *argv[]) {
	QApplication app (argc, argv);

	app.setQuitOnLastWindowClosed (false);
	app.setWindowIcon (QIcon ("icon.svg"));

	TrayIcon trayIcon;

	return app.exec ();
}

