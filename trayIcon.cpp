#include "trayIcon.h"
#include "common.h"

TrayIcon::TrayIcon () : QSystemTrayIcon () {
	// Create trayicon
	createMenu ();

	// Show it permanently
	show ();
}

void TrayIcon::createMenu (void) {
	// create menu
	mContext = new QMenu;
	
	mAbout = new QAction ("&About...", this);
	QObject::connect (mAbout, SIGNAL (triggered ()), this, SLOT (about ()));

	mExit = new QAction ("&Exit", this);
	QObject::connect (mExit, SIGNAL (triggered ()), qApp, SLOT (quit ()));

	mContext->addAction (mAbout);
	mContext->addAction (mExit);

	// set icon
	setContextMenu (mContext);
	setIcon (qApp->windowIcon ());

	// connect internal signals
	QObject::connect (this, SIGNAL (activated (QSystemTrayIcon::ActivationReason)),
		this, SLOT (wasClicked (QSystemTrayIcon::ActivationReason)));	
}

TrayIcon::~TrayIcon () {
	delete mContext;
}

void TrayIcon::wasClicked (QSystemTrayIcon::ActivationReason reason) {
	if (reason == QSystemTrayIcon::Trigger)
		emit mainWindowToggled ();
}

void TrayIcon::about (void) {
	QMessageBox::about (0,
		"About...",
		"Program written by Francois Gindraud"
	);
	// TODO better
}

