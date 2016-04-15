#include "discovery.h"
#include "settings.h"
#include "localshare.h"

#include <QApplication>

int main (int argc, char *argv[]) {
	qRegisterMetaType<Discovery::Peer> ();

	// Enable usage of QSettings default constructor
	QCoreApplication::setOrganizationName (Const::app_name);
	QCoreApplication::setApplicationName (Const::app_name);

	QApplication app (argc, argv);
	//app.setWindowIcon (Icon::app ());
	
	Discovery::Service serv {Settings::Username ().get (), Const::service_name, (quint16) (40000 + argc)};
	Discovery::Browser browser {Const::service_name};

	return app.exec ();
}

