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
	mWaitingFileList = new QVBoxLayout;
	mMainVbox->addLayout (mWaitingFileList);

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
	QStringList addedFiles = QFileDialog::getOpenFileNames (this);

	foreach (QString file, addedFiles)
		mWaitingFileList->addWidget (new WaitingForTransferFileWidget (file));
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

/* ------ File waiting ------ */

WaitingForTransferFileWidget::WaitingForTransferFileWidget (const QString & file) {
	// Save file info
	fileName = file;
	
	// Create widget
	mFileIconLabel = new IconLabel (Icon::file ());

	quint64 fileSize = QFileInfo (file).size ();
	QString description = QString ("%1 [%2]").arg (file, fileSizeToString (fileSize));
	mFileDescrLabel = new QLabel (description);

	mDeleteFile = new IconButton (Icon::closeAbort ());

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

