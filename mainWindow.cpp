#include "mainWindow.h"

/* ----- MainWindow ----- */

MainWindow::MainWindow (Settings & settings, ZeroconfHandler & discoveryHandler) {
	(void) settings; (void) discoveryHandler;

	createMainWindow ();
}

void MainWindow::createMainWindow (void) {
	m_label = new QLabel ("Experimental text", this);

	// Window setup
	setWindowFlags (Qt::Tool);
	setWindowTitle (APP_NAME);
}

void MainWindow::closeEvent (QCloseEvent * event) {
	// Avoid window closure
	event->ignore ();

	// Hide window instead
	hide ();
}

MainWindow::~MainWindow () {
	delete m_label;
}

void MainWindow::toggled (void) {
	if (isVisible ())
		hide ();
	else
		show ();
}

