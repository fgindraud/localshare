#include "discovery.h"
#include "settings.h"
#include "style.h"
#include "transfer.h"
#include "localshare.h"

#include <QApplication>
#include <QTimer>

int main (int argc, char * argv[]) {
	// Enable usage of QSettings default constructor
	QCoreApplication::setOrganizationName (Const::app_name);
	QCoreApplication::setApplicationName (Const::app_name);

	QApplication app (argc, argv);
	app.setWindowIcon (Icon::app ());

	auto username = Settings::Username ().get ();
	if (argc >= 2)
		username += argv[1];

	Transfer::Server server;
	Discovery::Service serv{username, Const::service_name, server.port ()};

	QObject::connect (&serv, &Discovery::Service::registered, [&](QString name) {
		auto b = new Discovery::Browser{name, Const::service_name, &serv};

		QObject::connect (b, &Discovery::Browser::added,
		                  [](const Discovery::Peer & peer) { qDebug () << "added" << peer; });
		QObject::connect (b, &Discovery::Browser::removed,
		                  [](const Discovery::Peer & peer) { qDebug () << "removed" << peer; });
	});

	QTimer::singleShot (10 * 1000, &app, &QApplication::quit);

	Transfer::Upload upload (Discovery::Peer {"user", "hostname", QHostAddress::LocalHost, server.port ()});

	return app.exec ();
}
