#include "mainWindow.h"

/* ----- MainWindow ----- */

MainWindow::MainWindow (ZeroconfHandler * discoveryHandler) : mTray (0) {
	createMainWindow ();
	connectSignals (discoveryHandler);

	if (Settings::useSystemTray ())
		trayIconEnabledAdditionalSetup ();

	show ();
}

void MainWindow::createMainWindow (void) {
	// WIndow
	setWindowFlags (Qt::Window);
	setWindowTitle (APP_NAME);
	
	// Main layout
	mMainVbox = new QVBoxLayout;
	setLayout (mMainVbox);
	
	// First line
	mHeaderHbox = new QHBoxLayout;
	mMainVbox->addLayout (mHeaderHbox);
	
	mHeaderFilterTextEdit = new QLineEdit;
	mHeaderFilterTextEdit->setToolTip ("Filter peers");
	mHeaderHbox->addWidget (mHeaderFilterTextEdit, 1);

	mHeaderAddButton = new IconButton (Icon::openFile (), "Choose file...");
	mHeaderHbox->addWidget (mHeaderAddButton);
	
	mHeaderSettingsButton = new IconButton (Icon::settings (), "Settings...");
	mHeaderHbox->addWidget (mHeaderSettingsButton);

	// Second
	mSendFileArea = new SendFileArea;
	mMainVbox->addWidget (mSendFileArea);

	// Peers
	mPeerList = new PeerListWidget;
	mMainVbox->addWidget (mPeerList, 1);
}

void MainWindow::connectSignals (ZeroconfHandler * discoveryHandler) {
	// Peer list updates
	QObject::connect (discoveryHandler, SIGNAL (addPeer (ZeroconfPeer &)),
			mPeerList, SLOT (addPeer (ZeroconfPeer &)));
	QObject::connect (discoveryHandler, SIGNAL (removePeer (QString &)),
			mPeerList, SLOT (removePeer (QString &)));

	// Peer filter
	QObject::connect (mHeaderFilterTextEdit, SIGNAL (textChanged (const QString &)),
			mPeerList, SLOT (filterPeers (const QString &)));

	// Open File & Settings
	QObject::connect (mHeaderAddButton, SIGNAL (clicked ()),
			this, SLOT (addFiles ()));
	QObject::connect (mHeaderSettingsButton, SIGNAL (clicked ()),
			this, SLOT (invokeSettings ()));
}

void MainWindow::trayIconEnabledAdditionalSetup (void) {
	// Create tray icon, connect signals
	mTray = new TrayIcon (this);
	
	// Prevent window deletion on close : will only hide it
	setAttribute (Qt::WA_DeleteOnClose, false);

	// Prevent application from quitting when window is closed
	setAttribute (Qt::WA_QuitOnClose, false);
}

void MainWindow::windowVisibilityToggled (void) {
	setVisible (not isVisible ());
}

void MainWindow::addFiles (void) {
	// Get files from dialog
	QStringList addedFiles = QFileDialog::getOpenFileNames (this);

	// Put them in holder area
	foreach (QString file, addedFiles)
		mSendFileArea->addFile (file);
}

void MainWindow::invokeSettings (void) {
	//TODO
}

/* ------ Tray icon ------ */
TrayIcon::TrayIcon (MainWindow * window) : QSystemTrayIcon () {
	createMenu ();
	connectToWindow (window);
	show ();
}

TrayIcon::~TrayIcon () { delete mContext; }

void TrayIcon::createMenu (void) {
	// Select tray icon
	setIcon (Icon::app ());
	
	// Create menu
	mContext = new QMenu;
	setContextMenu (mContext);
	
	mOpenFile = new QAction ("Open file...", this);
	mContext->addAction (mOpenFile);

	mSettings = new QAction ("Settings...", this);
	mContext->addAction (mSettings);
	
	mExit = new QAction ("Quit", this);
	mContext->addAction (mExit);

	// Connect internal signal
	QObject::connect (this, SIGNAL (activated (QSystemTrayIcon::ActivationReason)),
		this, SLOT (wasClicked (QSystemTrayIcon::ActivationReason)));	
	
	// Exit
	QObject::connect (mExit, SIGNAL (triggered ()), qApp, SLOT (quit ()));
}

void TrayIcon::connectToWindow (MainWindow * window) {
	// Open file
	// Show main window before opening dialog
	QObject::connect (mOpenFile, SIGNAL (triggered ()),
			window, SLOT (show ()));
	QObject::connect (mOpenFile, SIGNAL (triggered ()),
			window, SLOT (addFiles ()));
	
	// Settings (similar)
	QObject::connect (mSettings, SIGNAL (triggered ()),
			window, SLOT (show ()));
	QObject::connect (mSettings, SIGNAL (triggered ()),
			window, SLOT (invokeSettings ()));
	
	// Add minimize-to-tray system
	QObject::connect (this, SIGNAL (mainWindowToggled ()),
			window, SLOT (windowVisibilityToggled ()));
}

void TrayIcon::wasClicked (QSystemTrayIcon::ActivationReason reason) {
	// Only propagate click event on simple clicks
	if (reason == QSystemTrayIcon::Trigger)
		emit mainWindowToggled ();
}

/* ------ Send File Area ------ */
SendFileArea::SendFileArea () : StyledFrame () {
	// Enable drops
	setAcceptDrops (true);

	// Main vbox
	mLayout = new QVBoxLayout;
	setLayout (mLayout);
}

void SendFileArea::dragEnterEvent (QDragEnterEvent * event) {
	// Accept only Urls
	if (event->mimeData ()->hasUrls ())
		event->acceptProposedAction ();
}

void SendFileArea::dropEvent (QDropEvent * event) {
	// Accept only Urls pointing to local files
	if (event->mimeData ()->hasUrls ()) {
		foreach (QUrl url, event->mimeData ()->urls ())
			if (url.isLocalFile ())
				// Then add it.
				addFile (url.toLocalFile ());

		event->acceptProposedAction ();
	}
}

void SendFileArea::addFile (const QString filePath) {
	FileUtils::Size size;
	if (FileUtils::infoCheck (filePath, &size))
		mLayout->addWidget (new SendFileItem (filePath, size));
}

/* ------ Send file item ------ */
SendFileItem::SendFileItem (const QString file, FileUtils::Size size) :
	BoxWidgetWrapper (QBoxLayout::LeftToRight) {
	// Save filename
	mHoldFileName = file;
	
	// Gui
	mFileIconLabel = new IconLabel (Icon::file ());
	mBoxLayout->addWidget (mFileIconLabel);

	QString description = QString ("%1 [%2]").arg (file, FileUtils::sizeToString (size));
	mFileDescrLabel = new QLabel (description);
	mBoxLayout->addWidget (mFileDescrLabel, 1);

	mDelete = new IconButton (Icon::closeAbort ());
	mBoxLayout->addWidget (mDelete);

	// Delete this file holder when delete button is clicked
	QObject::connect (mDelete, SIGNAL (clicked ()),
			this, SLOT (deleteLater ()));
}
		
void SendFileItem::mousePressEvent (QMouseEvent * event) {
	// Save click position
	if (event->button () == Qt::LeftButton)
		mStartDragPos = event->pos ();
}

void SendFileItem::mouseMoveEvent (QMouseEvent * event) {
	// Check if left button is hold, and if we moved enough to start dragging
	if (not (event->buttons () & Qt::LeftButton))
		return;
	if ((event->pos () - mStartDragPos).manhattanLength () < QApplication::startDragDistance ())
		return;

	// Create draggable info : one Url pointing to the local file.
	QList< QUrl > urlSingleton;
	urlSingleton << QUrl ().fromLocalFile (mHoldFileName);
	QMimeData * mimeData = new QMimeData;
	mimeData->setUrls (urlSingleton);

	// Create drag class, and set a file icon.
	QDrag * drag = new QDrag (this);
	drag->setMimeData (mimeData);
	drag->setPixmap (Icon::file ().pixmap (DRAG_ICON_SIZE));

	// Exec drag
	drag->exec (Qt::CopyAction);
}

