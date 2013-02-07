/*
 * Peer widgets.
 * 
 */
#ifndef H_PEERWIDGETS
#define H_PEERWIDGETS

#include "network.h"
#include "common.h"
#include "miscWidgets.h"

#include <QtGui>
#include <QList>

class PeerListWidget;
class PeerWidget;
class TransferWidget;

class InTransferWidget;
class OutTransferWidget;

class PeerHandler : public ZeroconfPeer {
	public:
		PeerHandler (ZeroconfPeer & peerBase) :
			ZeroconfPeer (peerBase), mView (0)
		{}
		
		void setView (PeerWidget * widget);
		QWidget * view (void);

	private:
		PeerWidget * mView;
};

class PeerListWidget : public QScrollArea {
	Q_OBJECT

	public:
		typedef QList<PeerHandler *> PeerList;

		PeerListWidget ();

	public slots:
		void addPeer (ZeroconfPeer & peer);
		void removePeer (QString & peer);

		void filterPeers (const QString & namePart);
	
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

	protected:
		void dragEnterEvent (QDragEnterEvent * event);
		void dropEvent (QDropEvent * event);

	private:

		QLabel * mPeerNetworkInfo;

		QVBoxLayout * mLayout;
};

/*
 * Transfer widget
 */

class TransferWidget : public StyledFrame {
	public:
		enum Status {
			Waiting, Transfering, Finished
		};

		TransferWidget ();

	protected:

		//TransferHandler * mTransferHandler;

		// Left info
		IconLabel * mTransferTypeIcon;
		QLabel * mFileDescr;

		// Steps
		BoxWidgetWrapper * mWaitingWidget;
		
		QProgressBar * mTransferingProgressBar;
		
		QLabel * mFinishedStatus;

		QHBoxLayout * mStepsLayout;

		// Right
		IconButton * mCloseAbortButton;

		QHBoxLayout * mMainLayout;

		// Private functions
		void setStatus (Status status);	
};

class InTransferWidget : public TransferWidget {
	public:
		InTransferWidget ();
		
	private:
		// Additionnal widgets
		IconButton * mAcceptButton;
};

class OutTransferWidget : public TransferWidget {
	public:
		OutTransferWidget ();
};

#endif

