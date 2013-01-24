#include "trayIcon.h"

#include "common.h"

TrayIcon::TrayIcon () : QSystemTrayIcon () {
	// create menu
	m_context = new QMenu ();
	
	m_about = new QAction (tr ("&About..."), this);
	QObject::connect (m_about, SIGNAL (triggered ()), this, SLOT (about ()));

	m_exit = new QAction (tr ("&Exit"), this);
	QObject::connect (m_exit, SIGNAL (triggered ()), qApp, SLOT (quit ()));

	m_context->addAction (m_about);
	m_context->addAction (m_exit);

	// Add Drag & Drop capabilities
	// TODO

	// set icon and show
	setContextMenu (m_context);
	setIcon (qApp->windowIcon ()); 
	show ();
}

TrayIcon::~TrayIcon () {
	delete m_context;
}

void TrayIcon::about (void) {
	QMessageBox::about (0,
			tr ("About..."),
			tr ("Program written by Francois Gindraud"));
	// TODO better
}

