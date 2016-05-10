#ifndef PEER_LIST_H
#define PEER_LIST_H

#include "localshare.h"
#include "struct_item_model.h"

#include <QMimeData>
#include <QUrl>

class PeerItem : public StructItem {
	Q_OBJECT

public:
	// View fields
	enum Field { UsernameField, HostnameField, AddressField, PortField, NbFields };

	const Peer peer;

signals:
	void request_upload (Peer peer, QString filepath);

public:
	PeerItem (const Peer & peer, QObject * parent = nullptr)
	    : StructItem (NbFields, parent), peer (peer) {}

	// Model
	Qt::ItemFlags flags (int field) const Q_DECL_OVERRIDE {
		return StructItem::flags (field) | Qt::ItemIsDropEnabled;
	}

	QVariant data (int field, int role) const Q_DECL_OVERRIDE {
		if (role != Qt::DisplayRole)
			return {};
		switch (field) {
		case UsernameField:
			return peer.username;
		case HostnameField:
			return peer.hostname;
		case AddressField:
			return peer.address.toString ();
		case PortField:
			return peer.port;
		default:
			return {};
		}
	}

	bool canDropMimeData (const QMimeData * mimedata, Qt::DropAction, int) const Q_DECL_OVERRIDE {
		return mimedata->hasUrls ();
	}
	bool dropMimeData (const QMimeData * mimedata, Qt::DropAction action, int field) Q_DECL_OVERRIDE {
		if (!canDropMimeData (mimedata, action, field))
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
	PeerListModel (QObject * parent = nullptr) : StructItemModel (PeerItem::NbFields, parent) {}

	void delete_peer (const QString & username) {
		for (auto i = 0; i < size (); ++i) {
			auto item = at_t<PeerItem *> (i);
			Q_ASSERT (item != nullptr);
			if (item->peer.username == username) {
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
		switch (section) {
		case PeerItem::UsernameField:
			return tr ("Username");
		case PeerItem::HostnameField:
			return tr ("Hostname");
		case PeerItem::AddressField:
			return tr ("Ip address");
		case PeerItem::PortField:
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
