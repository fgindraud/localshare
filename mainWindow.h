/*
 * Main window.
 * Heavily uses drag & drop
 */
#ifndef H_MAINWINDOW
#define H_MAINWINDOW

#include "network.h"
#include "trayIcon.h"
#include "common.h"

#include <QtGui>
#include <QList>

class PeerListWidget;
class PeerWidget;
class TransferWidget;

class InTransferWidget;
class OutTransferWidget;

class MainWindow : public QWidget {
	Q_OBJECT

	public:
		MainWindow (Settings & settings,
				ZeroconfHandler * discoveryHandler,
				TrayIcon * trayIcon);

		void closeEvent (QCloseEvent * event);
		void changeEvent (QEvent * event);

	public slots:
		void toggled (void);

	private:
		void createMainWindow (void);


		QLineEdit * mHeaderFilterTextEdit;
		QPushButton * mHeaderAddButton;
		QPushButton * mHeaderSettingsButton;

		QHBoxLayout * mHeaderHbox;

		PeerListWidget * mPeerList;

		QVBoxLayout * mMainVbox;
};

class PeerHandler : public ZeroconfPeer {
	public:
		PeerHandler (ZeroconfPeer & peerBase) :
			ZeroconfPeer (peerBase), view (0)
		{}
		
		void setView (PeerWidget * widget);
		void deleteView (void);

	private:
		PeerWidget * view;

		// Nothing yet
		// TODO list of connections, etc
};

class PeerListWidget : public QScrollArea {
	Q_OBJECT

	public:
		typedef QList<PeerHandler *> PeerList;

		PeerListWidget ();

	public slots:
		void addPeer (ZeroconfPeer peer);
		void removePeer (QString peer);
	
	private:
		void addPeerInternal (int index, ZeroconfPeer & peer);

		QWidget * mScrollInternal;
		QVBoxLayout * mScrollInternalLayout;

		PeerList mPeerList;
};

class PeerWidget : public QGroupBox {
	Q_OBJECT

	public:
		PeerWidget (PeerHandler * peer);

	private:

		QLabel * mPeerNetworkInfo;

		QVBoxLayout * mLayout;
};

/*
 * Transfer widget
 */

class TransferWidget : public QFrame {
	public:
		TransferWidget ();

	protected:

		TransferHandler * mTransferHandler;

		// Left info
		QLabel * mTransferTypeIcon;
		QLabel * mFileDescr;

		// Steps
		QHBoxLayout * mWaitingWidgets;
		
		QProgressBar * mTransferingProgressBar;
		
		QLabel * mFinishedStatus;

		QHBoxLayout * mStepsLayout;

		// Right
		QPushButton * mCloseAbortButton;

		QHBoxLayout * mMainLayout;

		// Private functions
			
};

class InTransferWidget : public TransferWidget {
	public:
		InTransferWidget ();
		
	private:
		// Additionnal widgets
		QPushButton * mAcceptButton;
};

class OutTransferWidget : public TransferWidget {
	public:
		OutTransferWidget ();
};

#endif

