#include "trayIcon.h"

#include "common.h"

TrayIcon::TrayIcon () : QSystemTrayIcon () {
	createMenu ();
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

	// Add main window toggling on click
	// TODO

	// set icon and show
	setContextMenu (m_context);
	setIcon (qApp->windowIcon ()); 
}

TrayIcon::~TrayIcon () {
	delete m_context;
}

void TrayIcon::about (void) {
	QMessageBox::about (0,
		"About...",
		"Program written by Francois Gindraud"
	);
	// TODO better
}

