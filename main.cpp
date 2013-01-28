#include "trayIcon.h"
#include "network.h"
#include "mainWindow.h"

#include <QApplication>

void programWideSettingsInit (QApplication & app);

int main (int argc, char *argv[]) {
	// Main App
	QApplication app (argc, argv);

	if (QSystemTrayIcon::isSystemTrayAvailable ()) {
		programWideSettingsInit (app);

		Settings settings;

		// Network discovery
		ZeroconfHandler networkDiscoveryHandler (settings);

		// Create tray icon, and show it.
		TrayIcon trayIcon;
		
		// Main window
		MainWindow mainWindow (settings, &networkDiscoveryHandler, &trayIcon);
		
		// Start discovery
		networkDiscoveryHandler.start ();

		// Start gui
		return app.exec ();
	} else {
		Message::error (
			"No system tray",
			APP_NAME " requires a system tray system to work"
		);

		return app.exec ();
	}
}

void programWideSettingsInit (QApplication & app) {
	// register custom types for qt-event-loop
	qRegisterMetaType<ZeroconfPeer> ("ZeroconfPeer");
	
	// Using no main window, so disable stupid defaults
	app.setQuitOnLastWindowClosed (false);

	// Icon
	app.setWindowIcon (QIcon ("icon.svg"));
}

