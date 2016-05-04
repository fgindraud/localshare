#ifndef PEER_LIST_H
#define PEER_LIST_H

#include "localshare.h"
#include "struct_item_model.h"

#include <QMimeData>
#include <QUrl>

enum class PeerField { UserName, HostName, Address, Port, Num };

class PeerItem : public StructItem {
	Q_OBJECT

private:
	Peer peer;

signals:
	void request_upload (Peer peer, QString filepath);

public:
	PeerItem (const Peer & peer, QObject * parent = nullptr)
	    : StructItem (int(PeerField::Num), parent), peer (peer) {}

	const Peer & get_peer (void) const { return peer; }

	// Model
	Qt::ItemFlags flags (int elem) const Q_DECL_OVERRIDE {
		return StructItem::flags (elem) | Qt::ItemIsDropEnabled;
	}

	QVariant data (int elem, int role) const Q_DECL_OVERRIDE {
		if (role != Qt::DisplayRole)
			return {};
		switch (PeerField (elem)) {
		case PeerField::UserName:
			return peer.username;
		case PeerField::HostName:
			return peer.hostname;
		case PeerField::Address:
			return peer.address.toString ();
		case PeerField::Port:
			return peer.port;
		default:
			return {};
		}
	}

	bool canDropMimeData (const QMimeData * mimedata, Qt::DropAction, int) const Q_DECL_OVERRIDE {
		return mimedata->hasUrls ();
	}
	bool dropMimeData (const QMimeData * mimedata, Qt::DropAction action, int elem) Q_DECL_OVERRIDE {
		if (!canDropMimeData (mimedata, action, elem))
			return false;
		for (auto & url : mimedata->urls ())
			if (url.isValid () && url.isLocalFile ())
				emit request_upload (peer, url.toLocalFile ());
		return true;
	}
};

class PeerListModel : public StructItemModel {
	Q_OBJECT

public:
	PeerListModel (QObject * parent = nullptr) : StructItemModel (int(PeerField::Num), parent) {}

	void delete_peer (const QString & username) {
		for (auto i = 0; i < size (); ++i) {
			auto item = at_t<PeerItem *> (i);
			Q_ASSERT (item != nullptr);
			if (item->get_peer ().username == username) {
				item->deleteLater ();
				return;
			}
		}
		qCritical () << "PeerListModel: peer not found:" << username;
	}

	QVariant headerData (int section, Qt::Orientation orientation,
	                     int role = Qt::DisplayRole) const Q_DECL_OVERRIDE {
		if (!(role == Qt::DisplayRole && orientation == Qt::Horizontal))
			return {};
		switch (PeerField (section)) {
		case PeerField::UserName:
			return tr ("Username");
		case PeerField::HostName:
			return tr ("Hostname");
		case PeerField::Address:
			return tr ("Ip address");
		case PeerField::Port:
			return tr ("Tcp port");
		default:
			return {};
		}
	}

	QStringList mimeTypes (void) const Q_DECL_OVERRIDE {
		QStringList list{"text/uri-list"};
		list.append (StructItemModel::mimeTypes ());
		return list;
	}
	Qt::DropActions supportedDropActions (void) const Q_DECL_OVERRIDE {
		return Qt::CopyAction | StructItemModel::supportedDropActions ();
	}
};

#endif
