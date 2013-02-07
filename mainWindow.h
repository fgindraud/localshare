/*
 * Main window.
 */
#ifndef H_MAINWINDOW
#define H_MAINWINDOW

#include "network.h"
#include "peerWidgets.h"
#include "miscWidgets.h"
#include "common.h"

#include <QtGui>

class MainWindow;
class TrayIcon;
class SendFileArea;
class SendFileItem;

/*
 * Main window
 * 
 * Maintains a list of peers, and the list of transfers for each peers.
 * Has a peer filter (by name) to show a subset of connected peers.
 *
 * Sending a file to a peer is done by drag&drop-ing of a file in the peer area.
 * Also manage a "buffer" for sent files : you can add a file to this buffer
 * with "open file", and then drag&drop it from there.
 */
class MainWindow : public QWidget {
	Q_OBJECT

	public:
		// Constructor : takes the discovery manager to connect signals.
		MainWindow (ZeroconfHandler * discoveryHandler);

	public slots:
		// Called by tray icon when it is clicked ; toggle window visibility (minimize-to-tray)
		void windowVisibilityToggled (void);

		// Invoke "open file" dialog
		void addFiles (void);

		// Invoke settings dialog
		void invokeSettings (void);

	private:
		// Window construction
		void createMainWindow (void);
		void connectSignals (ZeroconfHandler * discoveryHandler);
		void trayIconEnabledAdditionalSetup (void);

	private:
		// Optionnal tray icon
		TrayIcon * mTray;

		/*
		 * Window structure
		 */
		QVBoxLayout * mMainVbox;
	
		// First line : peer filter, and settings/open file buttons	
		QHBoxLayout * mHeaderHbox;
		QLineEdit * mHeaderFilterTextEdit;
		IconButton * mHeaderAddButton;
		IconButton * mHeaderSettingsButton;

		// Second : list of buffered files
		SendFileArea * mSendFileArea;

		// Third : List of peers
		PeerListWidget * mPeerList;
};

/*
 * TrayIcon (optionnal)
 * 
 *	When enabled, is able to show/hide the main window on click.
 * Houses links to utilities (settings, open file).
 */
class TrayIcon : public QSystemTrayIcon {
	Q_OBJECT

	public:
		TrayIcon (MainWindow * window);
		~TrayIcon ();

	signals:
		// Emitted when tray icon is clicked
		void mainWindowToggled (void);

	private slots:
		// Filter activation reasons before emitting mainWindowToggled
		void wasClicked (QSystemTrayIcon::ActivationReason reason);

	private:
		// Creation
		void createMenu (void);
		void connectToWindow (MainWindow * window);

		/*
		 * Structure : linear menu
		 * Exit quits the program
		 */
		QMenu * mContext;

		QAction * mOpenFile;
		QAction * mSettings;
		QAction * mExit;
};

/*
 * SendFileArea
 * Used to store files before they can be drag&drop-ed to be sent.
 * Files loaded with "open file" will end up there.
 * Files can be dropped in there to be "stored".
 */
class SendFileArea : public StyledFrame {
	Q_OBJECT
	
	public:
		SendFileArea ();

		// Add a file to the list if the file is valid (exists, readable)
		void addFile (const QString filePath);

	protected:
		// Handle file drop
		void dragEnterEvent (QDragEnterEvent * event);
		void dropEvent (QDropEvent * event);

	private:
		/*
		 * Structure
		 */
		QVBoxLayout * mLayout;
};

/*
 * SendFileItem
 * Store one file. Draggable.
 */
class SendFileItem : public BoxWidgetWrapper {
	Q_OBJECT

	public:
		SendFileItem (const QString file, FileUtils::Size size);

	protected:
		// Draggable stuff
		void mousePressEvent (QMouseEvent * event);
		void mouseMoveEvent (QMouseEvent * event);

	private:
		// Drag & drop threshold holder variable
		QPoint mStartDragPos;

		// Complete filename given to widget we drop this in
		QString mHoldFileName;

		/*
		 * Structure
		 */
		IconLabel * mFileIconLabel;
		QLabel * mFileDescrLabel;
		IconButton * mDelete;
};



#endif

