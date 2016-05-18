#ifndef PEER_LIST_H
#define PEER_LIST_H

#include <QMimeData>
#include <QUrl>
#include <QStyledItemDelegate>
#include <QBrush>
#include <QHostInfo>

#include "localshare.h"
#include "struct_item_model.h"
#include "button_delegate.h"
#include "style.h"

namespace PeerList {

class Item : public StructItem {
	Q_OBJECT

public:
	// View fields
	enum Field { UsernameField, HostnameField, AddressField, PortField, NbFields };

	// Buttons
	enum Role { ButtonRole = ButtonDelegate::ButtonRole };
	enum Button { NoButton = 0x0, DeleteButton = 0x1 << 0 };

protected:
	Peer peer;

signals:
	void request_upload (Peer peer, QString filepath);

public:
	Item (const Peer & peer, QObject * parent = nullptr)
	    : StructItem (NbFields, parent), peer (peer) {}

	const Peer & get_peer (void) const { return peer; }
	virtual QString get_username (void) const { return peer.username; }

	// Model
	Qt::ItemFlags flags (int field) const Q_DECL_OVERRIDE {
		return StructItem::flags (field) | Qt::ItemIsDropEnabled;
	}

	QVariant data (int field, int role) const Q_DECL_OVERRIDE {
		switch (role) {
		case Qt::DisplayRole: {
			switch (field) {
			case UsernameField:
				return peer.username;
			case HostnameField:
				return peer.hostname;
			case AddressField:
				return peer.address.toString ();
			case PortField:
				return peer.port;
			}
		} break;
		}
		return {};
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

	virtual void button_clicked (int, Button) {}
};

class ManualItem : public Item {
	Q_OBJECT

	/* Manually added peer.
	 * It is editable in place, and can be deleted by a button.
	 * It is invalid if not enough information is filed to use it.
	 * Invalid state has a red background, cannot be selected nor accept drops.
	 */
public:
	ManualItem (QObject * parent = nullptr) : Item (Peer{}, parent) {}

	QString get_username (void) const Q_DECL_OVERRIDE {
		return {}; // Return invalid username to prevent matching by model
	}

	Qt::ItemFlags flags (int field) const Q_DECL_OVERRIDE {
		auto f = Item::flags (field) | Qt::ItemIsEditable;
		if (!is_valid ())
			f &= ~Qt::ItemIsSelectable;
		return f;
	}

	QVariant data (int field, int role) const Q_DECL_OVERRIDE {
		switch (role) {
		case Qt::EditRole: {
			return Item::data (field, Qt::DisplayRole); // TODO
		} break;
		case Qt::BackgroundRole: {
			if (is_valid ()) {
				return Item::data (field, role);
			} else {
				return QBrush (Qt::red);
			}
		} break;
		case ButtonRole: {
			if (field == UsernameField)
				return int(DeleteButton);
		} break;
		}
		return Item::data (field, role);
	}

	bool setData (int field, const QVariant & value, int role) Q_DECL_OVERRIDE {
		if (role != Qt::EditRole)
			return false;
		switch (field) {
		case UsernameField:
			peer.username = value.toString ();
			break;
		case HostnameField: {
			// Set hostname, lookup address
			peer.hostname = value.toString ();
			if (!peer.hostname.isEmpty ())
				QHostInfo::lookupHost (peer.hostname, this, SLOT (address_lookup_complete (QHostInfo)));
			break;
		}
		case AddressField: {
			// Set address, lookup hostname
			if (!peer.address.setAddress (value.toString ()))
				return false;
			if (!peer.address.isNull ())
				QHostInfo::lookupHost (peer.address.toString (), this,
				                       SLOT (hostname_lookup_complete (QHostInfo)));
			break;
		}
		case PortField:
			peer.port = value.toInt ();
			break;
		}
		edited_data (field);
		return true;
	}

	bool canDropMimeData (const QMimeData * mimedata, Qt::DropAction action,
	                      int field) const Q_DECL_OVERRIDE {
		return is_valid () && Item::canDropMimeData (mimedata, action, field);
	}

	void button_clicked (int, Button btn) Q_DECL_OVERRIDE {
		if (btn == DeleteButton)
			deleteLater ();
	}

private slots:
	void address_lookup_complete (const QHostInfo & info) {
		if (info.error () == QHostInfo::NoError && !info.addresses ().isEmpty ()) {
			peer.address = info.addresses ().first ();
			edited_data (AddressField);
		}
	}
	void hostname_lookup_complete (const QHostInfo & info) {
		if (info.error () == QHostInfo::NoError && !info.hostName ().isEmpty ()) {
			peer.hostname = info.hostName ();
			edited_data (HostnameField);
		}
	}

private:
	bool is_valid (void) const {
		return !peer.username.isEmpty () && !peer.address.isNull () && peer.port > 0;
	}
	void edited_data (int field) {
		emit data_changed (field, field, QVector<int>{Qt::DisplayRole, Qt::EditRole});
		emit data_changed (UsernameField, PortField, QVector<int>{Qt::BackgroundRole});
	}
};

class Model : public StructItemModel {
	Q_OBJECT

	// Model just sets header data and dispatches button clicks
public:
	Model (QObject * parent = nullptr) : StructItemModel (Item::NbFields, parent) {}

	void delete_peer (const QString & username) {
		for (auto i = 0; i < size (); ++i) {
			auto item = at_t<Item *> (i);
			Q_ASSERT (item != nullptr);
			if (item->get_username () == username) {
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
		case Item::UsernameField:
			return tr ("Username");
		case Item::HostnameField:
			return tr ("Hostname");
		case Item::AddressField:
			return tr ("Ip address");
		case Item::PortField:
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

public slots:
	void button_clicked (const QModelIndex & index, int btn) {
		if (has_item (index))
			return get_item_t<Item *> (index)->button_clicked (index.column (), Item::Button (btn));
	}
};

class InnerDelegate : public QStyledItemDelegate {
public:
	InnerDelegate (QObject * parent = nullptr) : QStyledItemDelegate (parent) {}
	// TODO nicer delegate for editing peers: tab friendly, limit range of ports, extend qlineedits,
	// and provide clean rects in ButtonDelegate
};

class Delegate : public ButtonDelegate {
public:
	Delegate (QObject * parent = nullptr) : ButtonDelegate (parent) {
		set_inner_delegate (new InnerDelegate (this));
		supported_buttons << SupportedButton{Item::DeleteButton, Icon::delete_peer ()};
	}
};
}

#endif
