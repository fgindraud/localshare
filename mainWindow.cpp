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
			mPeerList, SLOT (removePeer (QString &)));

	QObject::connect (mHeaderFilterTextEdit, SIGNAL (textChanged (const QString &)),
			mPeerList, SLOT (filterPeers (const QString &)));
}

void MainWindow::createMainWindow (void) {
	// Header
	mHeaderFilterTextEdit = new QLineEdit;
	mHeaderFilterTextEdit->setToolTip ("Filter peers");

	// Add & settings buttons
	mHeaderAddButton = new QPushButton;
	mHeaderAddButton->setIcon (appIcons.fileIcon ());
	mHeaderAddButton->setToolTip ("Send file...");

	mHeaderSettingsButton = new QPushButton;
	mHeaderSettingsButton->setIcon (appIcons.settingsIcon ());
	mHeaderSettingsButton->setToolTip ("Settings");

	mHeaderHbox = new QHBoxLayout;
	mHeaderHbox->addWidget (mHeaderFilterTextEdit, 1);
	mHeaderHbox->addWidget (mHeaderAddButton);
	mHeaderHbox->addWidget (mHeaderSettingsButton);

	// Peer list
	mPeerList = new PeerListWidget;

	// Main vbox
	mMainVbox = new QVBoxLayout;
	mMainVbox->addLayout (mHeaderHbox);
	mMainVbox->addWidget (mPeerList, 1);

	// Window setup
	setLayout (mMainVbox);
	setWindowFlags (Qt::Window);
	setWindowTitle (APP_NAME);
	setAttribute (Qt::WA_DeleteOnClose, false); // minimize to tray
	show ();
}


void MainWindow::toggled (void) {
	if (isVisible ())
		hide ();
	else
		show ();
}

/* ------ PeerHandler ------ */

void PeerHandler::setView (PeerWidget * widget) {
	if (mView == 0)
		mView = widget;
}

QWidget * PeerHandler::view (void) {
	return mView;
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

void PeerListWidget::removePeer (QString & peer) {
	for (PeerList::iterator it = mPeerList.begin (); it != mPeerList.end (); ++it) {
		PeerHandler * peerHandler = *it;
		if (peerHandler->name == peer) {
			// Delete view
			peerHandler->view ()->deleteLater ();

			// Delete it from list
			mPeerList.erase (it);
			delete peerHandler;

			return;
		}
	}
}

void PeerListWidget::filterPeers (const QString & namePart) {
	for (PeerList::iterator it = mPeerList.begin (); it != mPeerList.end (); ++it) {
		PeerHandler * peerHandler = *it;
		
		// Show only matching peers
		if (peerHandler->name.contains (namePart, Qt::CaseInsensitive))
			peerHandler->view ()->show ();
		else
			peerHandler->view ()->hide ();
	}
}

/* ------ PeerWidget ------ */

PeerWidget::PeerWidget (PeerHandler * peer) :
	QGroupBox ()
{
	// Finish connection
	peer->setView (this);

	// Add empty layout
	mLayout = new QVBoxLayout;

	//TEST
	mLayout->addWidget (new InTransferWidget ());
	mLayout->addWidget (new OutTransferWidget ());
	
	// Title
	QString peerNetworkInfo = QString ("%1 : %2:%3 [%4]")
		.arg (peer->name)
		.arg (peer->address.toString ())
		.arg (peer->port)
		.arg (peer->hostname);
	setTitle (peerNetworkInfo);
	setLayout (mLayout);
}

/* ------ Transfer widget ------ */

TransferWidget::TransferWidget () : QFrame () {
	// Left
	mTransferTypeIcon = new QLabel;
	mTransferTypeIcon->setAlignment (Qt::AlignHCenter | Qt::AlignVCenter);
	mTransferTypeIcon->setMargin (1);

	mFileDescr = new QLabel; //TODO get from transfer widget

	// Steps
	mWaitingWidgets = new QHBoxLayout;
	mWaitingWidgets->addStretch (1);
	
	mTransferingProgressBar = new QProgressBar;

	mFinishedStatus = new QLabel;

	mStepsLayout = new QHBoxLayout;
	mStepsLayout->addLayout (mWaitingWidgets);
	mStepsLayout->addWidget (mTransferingProgressBar);
	mStepsLayout->addWidget (mFinishedStatus);

	// Button
	mCloseAbortButton = new QPushButton;
	mCloseAbortButton->setIcon (appIcons.closeAbortIcon ());
	mCloseAbortButton->setToolTip ("Close connection");

	// Main layout
	mMainLayout = new QHBoxLayout;
	mMainLayout->addWidget (mTransferTypeIcon);
	mMainLayout->addWidget (mFileDescr);
	mMainLayout->addLayout (mStepsLayout, 1);
	mMainLayout->addWidget (mCloseAbortButton);

	// Frame
	setFrameStyle (QFrame::StyledPanel | QFrame::Raised);
	setLayout (mMainLayout);

	// Connections

}

InTransferWidget::InTransferWidget () : TransferWidget () {
	// Set icon of label
	int size = mTransferTypeIcon->sizeHint ().height ();
	mTransferTypeIcon->setPixmap (appIcons.inboundIcon ().pixmap (size));
	mTransferTypeIcon->setToolTip ("Download");

	// Add accept button to waiting widget
	mAcceptButton = new QPushButton;
	mAcceptButton->setIcon (appIcons.acceptIcon ());
	mAcceptButton->setToolTip ("Accept connection");

	mWaitingWidgets->addWidget (mAcceptButton);
}

OutTransferWidget::OutTransferWidget () : TransferWidget () {
	// Set icon of label
	int size = mTransferTypeIcon->sizeHint ().height ();
	mTransferTypeIcon->setPixmap (appIcons.outboundIcon ().pixmap (size));
	mTransferTypeIcon->setToolTip ("Upload");
}
