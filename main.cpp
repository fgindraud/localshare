#include "discovery.h"

#include <QApplication>

int main (int argc, char *argv[]) {
	QApplication app (argc, argv);
	//app.setWindowIcon (Icon::app ());

	if (argc < 2)
		qFatal ("requires 1 argument");

	Discovery::Service serv {argv[1], Const::service_name, (quint16) (40000 + argc)};
	Discovery::Browser browser {Const::service_name};

	return app.exec ();
}

