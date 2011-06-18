#include "trayIcon.h"

TrayIcon::TrayIcon () : QSystemTrayIcon () {
	// create menu
	contextMenu = new QMenu ();
	
	aboutAction = new QAction (tr ("&About..."), this);
	connect (aboutAction, SIGNAL (triggered ()), this, SLOT (about ()));

	exitAction = new QAction (tr ("&Exit"), this);
	connect (exitAction, SIGNAL (triggered ()), qApp, SLOT (quit ()));

	contextMenu->addAction (aboutAction);
	contextMenu->addAction (exitAction);

	// set icon and show
	setContextMenu (contextMenu);
	setIcon (QIcon ("icon.svg")); // TODO icon as ressource
	show ();
}

TrayIcon::~TrayIcon () {
	delete contextMenu;
}

void TrayIcon::about (void) {
	QMessageBox::about (0,
			tr ("About..."),
			tr ("Program written by Francois Gindraud"));
	// TODO better
}

