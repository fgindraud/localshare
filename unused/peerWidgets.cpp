#include "peerWidgets.h"

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

