#include "common.h"
#include "network.h"
#include "mainWindow.h"

#include <QApplication>

void programWideSettingsInit (QApplication & app);

int main (int argc, char *argv[]) {
	// Main app, misc settings
	QApplication app (argc, argv);
	programWideSettingsInit (app);

	// Network discovery
	ZeroconfHandler networkDiscoveryHandler;

	// Main window
	MainWindow mainWindow (&networkDiscoveryHandler);

	// Start discovery
	networkDiscoveryHandler.start ();

	return app.exec ();
}

void programWideSettingsInit (QApplication & app) {
	// register custom types for qt-event-loop
	qRegisterMetaType<ZeroconfPeer> ("ZeroconfPeer");

	// App default icon
	app.setWindowIcon (Icon::app ());
}

