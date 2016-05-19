#include "localshare.h"
#include "style.h"
#include "transfer.h"
#include "window.h"

#include <QApplication>

namespace Transfer {
Sizes sizes; // Precompute sizes
}

int main (int argc, char * argv[]) {
	// Enable usage of QSettings default constructor
	QCoreApplication::setOrganizationName (Const::app_name);
	QCoreApplication::setApplicationName (Const::app_name);

	QApplication app (argc, argv);
	app.setWindowIcon (Icon::app ());

	Window window;
	
	return app.exec ();
}
