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

class MainWindow : public QWidget {
	Q_OBJECT

	public:
		MainWindow (Settings & settings,
				ZeroconfHandler * discoveryHandler,
				TrayIcon * trayIcon);

		void closeEvent (QCloseEvent * event);

	public slots:
		void toggled (void);

	private:
		void createMainWindow (void);


		QLineEdit * mHeaderFilterTextEdit;		

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
};

#endif

