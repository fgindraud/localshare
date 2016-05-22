#include "localshare.h"
#include "style.h"
#include "window.h"

#include <QApplication>

namespace Transfer {
Sizes sizes; // Precompute sizes
}

int main (int argc, char * argv[]) {
	QApplication app (argc, argv);
	
	// Enable usage of QSettings default constructor
	app.setOrganizationName (Const::app_name);
	app.setApplicationName (Const::app_name);

	// Other misc info
	app.setApplicationDisplayName (Const::app_display_name);
	app.setApplicationVersion (Const::app_version);

	// Start app, set icons
	app.setWindowIcon (Icon::app ());
	Window window;
	
	return app.exec ();
}
