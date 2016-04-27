#ifndef PEER_LIST_H
#define PEER_LIST_H

#include "localshare.h"
#include "discovery.h"

#include <QAbstractListModel>
#include <QMimeData>
#include <QUrl>

class PeerListModel : public QAbstractListModel {
	Q_OBJECT

	/* List of peers (using Qt model-view).
	 * Is automatically updated by discovery browser.
	 */
private:
	QList<Peer> peer_list;

signals:
	void request_upload (Peer, QString);

public:
	PeerListModel (const QString & our_username, QObject * parent = nullptr)
	    : QAbstractListModel (parent) {
		auto browser = new Discovery::Browser (our_username, Const::service_name, this);
		connect (browser, &Discovery::Browser::added, this, &PeerListModel::peer_added);
		connect (browser, &Discovery::Browser::removed, this, &PeerListModel::peer_removed);

		// FIXME remove
		peer_list << Peer{"NSA", "nsa.gov", QHostAddress ("192.44.29.1"), 42};
		peer_list << Peer{"ANSSI", "anssi.fr", QHostAddress ("8.8.8.8"), 1000};
	}

	// Peer access
	bool has_peer (const QModelIndex & index) const {
		return index.isValid () && index.column () == 0 && 0 <= index.row () &&
		       index.row () < peer_list.size ();
	}
	Peer & get_peer (const QModelIndex & index) { return peer_list[index.row ()]; }
	const Peer & get_peer (const QModelIndex & index) const { return peer_list.at (index.row ()); }

	// View interface
	int rowCount (const QModelIndex & = QModelIndex ()) const { return peer_list.size (); }

	QVariant data (const QModelIndex & index, int role = Qt::DisplayRole) const {
		if (!(role == Qt::DisplayRole && has_peer (index)))
			return {};
		auto & peer = get_peer (index);
		return QString ("%1 @ %2 [%3 ; %4]")
		    .arg (peer.username)
		    .arg (peer.hostname)
		    .arg (peer.address.toString ())
		    .arg (peer.port);
	}

	QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const {
		if (!(role == Qt::DisplayRole && orientation == Qt::Horizontal && section == 0))
			return {};
		return tr ("Peers");
	}

	// Support drop of URIs
	Qt::ItemFlags flags (const QModelIndex & index) const {
		if (index.isValid ()) {
			return QAbstractListModel::flags (index) | Qt::ItemIsDropEnabled;
		} else {
			return 0;
		}
	}

	bool canDropMimeData (const QMimeData * mimedata, Qt::DropAction, int, int,
	                      const QModelIndex &) const {
		// do not check drop position : it seems to block updates of status
		return mimedata->hasUrls ();
	}

	bool dropMimeData (const QMimeData * mimedata, Qt::DropAction action, int row, int column,
	                   const QModelIndex & parent) {
		if (!canDropMimeData (mimedata, action, row, column, parent))
			return false;
		if (action == Qt::IgnoreAction)
			return true; // As in the tutorial... dunno if useful
		// Drop on item <=> row=col=-1, parent.row/col indicates position
		if (!(row == -1 && column == -1 && parent.isValid ()))
			return false;
		if (!has_peer (parent))
			return false;
		auto & peer = get_peer (parent);
		for (auto & url : mimedata->urls ())
			if (url.isValid () && url.isLocalFile ())
				emit request_upload (peer, url.toLocalFile ());
		return true;
	}

private slots:
	void peer_added (const Peer & peer) {
		// Add to end
		beginInsertRows (QModelIndex (), peer_list.size (), peer_list.size ());
		peer_list << peer;
		endInsertRows ();
	}
	void peer_removed (const QString & username) {
		// Find
		for (auto i = 0; i < peer_list.size (); ++i) {
			if (peer_list.at (i).username == username) {
				beginRemoveRows (QModelIndex (), i, i);
				peer_list.removeAt (i);
				endRemoveRows ();
				return;
			}
		}
		qCritical () << "PeerListModel: peer not found:" << username;
	}
};

#endif
