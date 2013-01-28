#include "mainWindow.h"

/* ----- MainWindow ----- */

MainWindow::MainWindow (Settings & settings,
		ZeroconfHandler * discoveryHandler, TrayIcon * trayIcon) {
	(void) settings; 
	createMainWindow ();

	// Connections
	QObject::connect (trayIcon, SIGNAL (mainWindowToggled ()),
			this, SLOT (toggled ()));

	QObject::connect (discoveryHandler, SIGNAL (addPeer (ZeroconfPeer)),
			mPeerList, SLOT (addPeer (ZeroconfPeer)));

	QObject::connect (discoveryHandler, SIGNAL (removePeer (QString)),
			mPeerList, SLOT (removePeer (QString)));
}

void MainWindow::createMainWindow (void) {
	// Header
	mHeaderFilterTextEdit = new QLineEdit;
	mHeaderFilterTextEdit->setToolTip ("Filter peers");

	mHeaderHbox = new QHBoxLayout;
	mHeaderHbox->addWidget (mHeaderFilterTextEdit);

	// Peer list
	mPeerList = new PeerListWidget;

	// Main vbox
	mMainVbox = new QVBoxLayout;
	mMainVbox->addLayout (mHeaderHbox);
	mMainVbox->addWidget (mPeerList, 1);

	// Window setup
	setLayout (mMainVbox);
	setWindowFlags (Qt::Tool);
	setWindowTitle (APP_NAME);
}

void MainWindow::closeEvent (QCloseEvent * event) {
	// Avoid window closure
	event->ignore ();

	// Hide window instead
	hide ();
}

void MainWindow::toggled (void) {
	if (isVisible ())
		hide ();
	else
		show ();
}

/* ------ PeerHandler ------ */

void PeerHandler::setView (PeerWidget * widget) {
	if (view == 0)
		view = widget;
}

void PeerHandler::deleteView (void) {
	if (view != 0)
		view->deleteLater ();
}

/* ------ PeerListWidget ----- */

PeerListWidget::PeerListWidget () {
	mScrollInternalLayout = new QVBoxLayout;
	mScrollInternalLayout->addStretch (1); // Spacer

	mScrollInternal = new QWidget;
	mScrollInternal->setSizePolicy (QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
	mScrollInternal->setLayout (mScrollInternalLayout);
	
	setWidget (mScrollInternal);
	setWidgetResizable (true);
	setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
}

void PeerListWidget::addPeer (ZeroconfPeer peer) {
	// Insert new peer in alphabetical order
	for (int i = 0; i < mPeerList.size (); ++i) {
		QString tested = mPeerList.at (i)->name;
		if (peer.name < tested) {
			// Add peer here
			addPeerInternal (i, peer);
			return;
		}
		if (peer.name == tested)
			// Drop duplicates, if any
			return;
	}

	// Add peer at end of the list
	addPeerInternal (mPeerList.size (), peer);
}

void PeerListWidget::addPeerInternal (int index, ZeroconfPeer & peer) {
	PeerHandler * handler = new PeerHandler (peer);

	// Insert it in model list
	mPeerList.insert (index, handler);
	
	// Insert it in view list
	PeerWidget * widget = new PeerWidget (handler);
	mScrollInternalLayout->insertWidget (index, widget);
}

void PeerListWidget::removePeer (QString peer) {
	for (int i = 0; i < mPeerList.size (); ++i) {
		PeerHandler * peerHandler = mPeerList[i];
		if (peerHandler->name == peer) {
			// Delete view
			peerHandler->deleteView ();

			// Delete it from list
			mPeerList.removeAt (i);
			delete peerHandler;

			return;
		}
	}
}

/* ------ PeerWidget ------ */

PeerWidget::PeerWidget (PeerHandler * peer) :
	QGroupBox (peer->name)
{
	// Finish connection
	peer->setView (this);

	//setSizePolicy (QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	QVBoxLayout * l = new QVBoxLayout;
	//l->setSizeConstraint (QLayout::SetMinimumSize);
	l->addWidget (new QLabel (peer->address.toString () + "|" + peer->hostname));
	l->addWidget (new QLabel (QString ("Port : %1").arg (peer->port)));
	//l->addStretch (1);
	setLayout (l);
}

