#include "trayIcon.h"
#include "common.h"

#include <QApplication>

int main (int argc, char *argv[]) {
	QApplication app (argc, argv);

	if (QSystemTrayIcon::isSystemTrayAvailable ()) {
		// Using no main window, so disable stupid defaults
		app.setQuitOnLastWindowClosed (false);

		// Icon
		app.setWindowIcon (QIcon ("icon.svg"));

		// Create tray icon, and show it.
		TrayIcon trayIcon;
		
		return app.exec ();
	} else {
		Message::error (
			"No system tray",
			APP_NAME " requires a system tray system to work"
		);

		return app.exec ();
	}
}

