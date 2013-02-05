#include "mainWindow.h"

/* ----- MainWindow ----- */

MainWindow::MainWindow (ZeroconfHandler * discoveryHandler, TrayIcon * trayIcon) {
	createMainWindow ();

	// Connections
	QObject::connect (trayIcon, SIGNAL (mainWindowToggled ()),
			this, SLOT (toggled ()));

	QObject::connect (discoveryHandler, SIGNAL (addPeer (ZeroconfPeer &)),
			mPeerList, SLOT (addPeer (ZeroconfPeer &)));

	QObject::connect (discoveryHandler, SIGNAL (removePeer (QString &)),
			mPeerList, SLOT (removePeer (QString &)));

	QObject::connect (mHeaderFilterTextEdit, SIGNAL (textChanged (const QString &)),
			mPeerList, SLOT (filterPeers (const QString &)));

	QObject::connect (mHeaderAddButton, SIGNAL (clicked ()),
			this, SLOT (addFiles ()));
}

void MainWindow::createMainWindow (void) {
	// Header
	mHeaderFilterTextEdit = new QLineEdit;
	mHeaderFilterTextEdit->setToolTip ("Filter peers");

	// Add & settings buttons
	mHeaderAddButton = new QPushButton;
	mHeaderAddButton->setIcon (Icon::openFile ());
	mHeaderAddButton->setToolTip ("Send file...");

	mHeaderSettingsButton = new QPushButton;
	mHeaderSettingsButton->setIcon (Icon::settings ());
	mHeaderSettingsButton->setToolTip ("Settings");

	mHeaderHbox = new QHBoxLayout;
	mHeaderHbox->addWidget (mHeaderFilterTextEdit, 1);
	mHeaderHbox->addWidget (mHeaderAddButton);
	mHeaderHbox->addWidget (mHeaderSettingsButton);

	// File list
	mWaitingFileList = new QVBoxLayout;

	// Peer list
	mPeerList = new PeerListWidget;

	// Main vbox
	mMainVbox = new QVBoxLayout;
	mMainVbox->addLayout (mHeaderHbox);
	mMainVbox->addLayout (mWaitingFileList);
	mMainVbox->addWidget (mPeerList, 1);

	// Window setup
	setLayout (mMainVbox);
	setWindowFlags (Qt::Window);
	setWindowTitle (APP_NAME);
	setAttribute (Qt::WA_DeleteOnClose, false); // minimize to tray
	show ();
}

void MainWindow::addFiles (void) {
	QStringList addedFiles = QFileDialog::getOpenFileNames (this);

	foreach (QString file, addedFiles)
		mWaitingFileList->addWidget (new WaitingForTransferFileWidget (file));
}

void MainWindow::toggled (void) {
	if (isVisible ())
		hide ();
	else
		show ();
}

/* ------ File waiting ------ */

WaitingForTransferFileWidget::WaitingForTransferFileWidget (const QString & file) {
	// Save file info
	fileName = file;
	
	// Create widget
	mFileIconLabel = new QLabel;
	int size = mFileIconLabel->sizeHint ().height ();
	mFileIconLabel->setPixmap (Icon::file ().pixmap (size));

	quint64 fileSize = QFileInfo (file).size ();
	QString description = QString ("%1 [%2]").arg (file, fileSizeToString (fileSize));
	mFileDescrLabel = new QLabel (description);

	mDeleteFile = new QPushButton;
	mDeleteFile->setIcon (Icon::closeAbort ());

	mLayout = new QHBoxLayout;
	mLayout->addWidget (mFileIconLabel);
	mLayout->addWidget (mFileDescrLabel, 1);
	mLayout->addWidget (mDeleteFile);

	setLayout (mLayout);
	setFrameStyle (QFrame::StyledPanel | QFrame::Raised);

	// Add delete handler
	QObject::connect (mDeleteFile, SIGNAL (clicked ()),
			this, SLOT (deleteLater ()));
}
		
void WaitingForTransferFileWidget::mousePressEvent (QMouseEvent * event) {
	if (event->button () == Qt::LeftButton)
		startDragPos = event->pos ();
}

void WaitingForTransferFileWidget::mouseMoveEvent (QMouseEvent * event) {
	// Stop if not relevant
	if (not (event->buttons () & Qt::LeftButton))
		return;
	if ((event->pos () - startDragPos).manhattanLength () < QApplication::startDragDistance ())
		return;

	// Build drag info
	QList< QUrl > url;
	url << QUrl ().fromLocalFile (fileName);

	QMimeData * mimeData = new QMimeData;
	mimeData->setUrls (url);

	QDrag * drag = new QDrag (this);
	drag->setMimeData (mimeData);
	drag->setPixmap (Icon::file ().pixmap (32));

	// Drag
	drag->exec (Qt::CopyAction);
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

void PeerListWidget::addPeer (ZeroconfPeer & peer) {
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
	foreach (PeerHandler * peerHandler, mPeerList) {
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
	setAcceptDrops (true);
}

void PeerWidget::dragEnterEvent (QDragEnterEvent * event) {
	if (event->mimeData ()->hasUrls ())
		event->acceptProposedAction ();
}

void PeerWidget::dropEvent (QDropEvent * event) {
	if (event->mimeData ()->hasUrls ()) {
		foreach (QUrl url, event->mimeData ()->urls ()) {
			if (url.isLocalFile ()) {
				// Retrieve only local files
				QString path = url.toLocalFile ();
				
				// For now, just qdebug ()
				qDebug () << "DropEvent :" << path;
			}
		}

		event->acceptProposedAction ();
	}
}

/* ------ Transfer widget ------ */

TransferWidget::TransferWidget () : QFrame () {
	// Left
	mTransferTypeIcon = new QLabel;
	mTransferTypeIcon->setAlignment (Qt::AlignHCenter | Qt::AlignVCenter);
	mTransferTypeIcon->setMargin (1);

	mFileDescr = new QLabel; //TODO get from transfer widget

	// Steps
	mWaitingLayout = new QHBoxLayout;
	mWaitingLayout->addStretch (1);

	mWaitingWidget = new QWidget;
	mWaitingWidget->setLayout (mWaitingLayout);
	
	mTransferingProgressBar = new QProgressBar;

	mFinishedStatus = new QLabel;

	mStepsLayout = new QHBoxLayout;
	mStepsLayout->addWidget (mWaitingWidget);
	mStepsLayout->addWidget (mTransferingProgressBar);
	mStepsLayout->addWidget (mFinishedStatus);

	// Button
	mCloseAbortButton = new QPushButton;
	mCloseAbortButton->setIcon (Icon::closeAbort ());
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

	// Start with waiting status
	setStatus (Waiting);

	// Connections

}

void TransferWidget::setStatus (TransferWidget::Status status) {
	mWaitingWidget->setVisible (status == Waiting);
	mTransferingProgressBar->setVisible (status == Transfering);
	mFinishedStatus->setVisible (status == Finished);
}

InTransferWidget::InTransferWidget () : TransferWidget () {
	// Set icon of label
	int size = mTransferTypeIcon->sizeHint ().height ();
	mTransferTypeIcon->setPixmap (Icon::inbound ().pixmap (size));
	mTransferTypeIcon->setToolTip ("Download");

	// Add accept button to waiting widget
	mAcceptButton = new QPushButton;
	mAcceptButton->setIcon (Icon::accept ());
	mAcceptButton->setToolTip ("Accept connection");

	mWaitingLayout->addWidget (mAcceptButton);
}

OutTransferWidget::OutTransferWidget () : TransferWidget () {
	// Set icon of label
	int size = mTransferTypeIcon->sizeHint ().height ();
	mTransferTypeIcon->setPixmap (Icon::outbound ().pixmap (size));
	mTransferTypeIcon->setToolTip ("Upload");
}
