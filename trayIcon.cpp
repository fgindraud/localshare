#include "trayIcon.h"
#include "common.h"

TrayIcon::TrayIcon (const MainWindow * window) : QSystemTrayIcon () {
	// Create trayicon
	createMenu ();

	// Link clicked signal to mainWindow
	QObject::connect (this, SIGNAL (mainWindowToggled ()),
			window, SLOT (toggled ()));

	// Show it permanently
	show ();
}

void TrayIcon::createMenu (void) {
	// create menu
	m_context = new QMenu ();
	
	m_about = new QAction ("&About...", this);
	QObject::connect (m_about, SIGNAL (triggered ()), this, SLOT (about ()));

	m_exit = new QAction ("&Exit", this);
	QObject::connect (m_exit, SIGNAL (triggered ()), qApp, SLOT (quit ()));

	m_context->addAction (m_about);
	m_context->addAction (m_exit);

	// set icon
	setContextMenu (m_context);
	setIcon (qApp->windowIcon ());

	// connect internal signals
	QObject::connect (this, SIGNAL (activated (QSystemTrayIcon::ActivationReason)),
		this, SLOT (wasClicked (QSystemTrayIcon::ActivationReason)));	
}

TrayIcon::~TrayIcon () {
	delete m_context;
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

