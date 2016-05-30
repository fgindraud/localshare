#pragma once
#ifndef GUI_PEER_LIST_H
#define GUI_PEER_LIST_H

#include <QBrush>
#include <QFlags>
#include <QHeaderView>
#include <QHostInfo>
#include <QMimeData>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QUrl>
#include <limits>

#include "core/discovery.h"
#include "core/localshare.h"
#include "gui/button_delegate.h"
#include "gui/struct_item_model.h"
#include "gui/style.h"

namespace PeerList {

/* Peer list item base.
 * Can have a button to delete it (for manual items).
 * It is invalid if not enough information is filed to use it.
 * Invalid state has a red background, cannot be selected nor accept drops.
 */
class Item : public StructItem {
	Q_OBJECT

public:
	// View fields
	enum Field { UsernameField, HostnameField, AddressField, PortField, NbFields };

	// Buttons
	enum Role { ButtonRole = ButtonDelegate::ButtonRole };
	enum Button { NoButton = 0x0, DeleteButton = 0x1 << 0 };
	Q_DECLARE_FLAGS (Buttons, Button);

protected:
	Peer peer;

signals:
	void request_upload (Peer peer, QString filepath);

public:
	Item (QObject * parent = nullptr) : StructItem (NbFields, parent), peer () {}

	const Peer & get_peer (void) const { return peer; }

	// Model
	Qt::ItemFlags flags (int field) const Q_DECL_OVERRIDE {
		auto f = StructItem::flags (field) | Qt::ItemIsDropEnabled;
		if (!is_valid ())
			f &= ~Qt::ItemIsSelectable;
		return f;
	}

	QVariant data (int field, int role) const Q_DECL_OVERRIDE {
		switch (role) {
		case Qt::DisplayRole:
		case Qt::EditRole: {
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
		case Qt::BackgroundRole: {
			if (!is_valid ())
				return QBrush (Qt::red);
		} break;
		}
		return {};
	}

	bool canDropMimeData (const QMimeData * mimedata, Qt::DropAction, int) const Q_DECL_OVERRIDE {
		return is_valid () && mimedata->hasUrls ();
	}
	bool dropMimeData (const QMimeData * mimedata, Qt::DropAction action, int field) Q_DECL_OVERRIDE {
		if (!canDropMimeData (mimedata, action, field))
			return false;
		for (auto & url : mimedata->urls ())
			if (url.isValid () && url.isLocalFile ())
				emit request_upload (peer, url.toLocalFile ());
		return true;
	}

	void button_clicked (int, Button btn) {
		if (btn == DeleteButton)
			deleteLater ();
	}

protected:
	void edited_data (int field) {
		emit data_changed (field, field, QVector<int>{Qt::DisplayRole, Qt::EditRole});
		emit data_changed (UsernameField, PortField, QVector<int>{Qt::BackgroundRole});
	}

private:
	bool is_valid (void) const {
		return !peer.username.isEmpty () && !peer.address.isNull () && peer.port > 0;
	}
};
Q_DECLARE_OPERATORS_FOR_FLAGS (Item::Buttons);

/* Item coming from Discovery.
 * TODO improve, handle DnsPeer updates
 */
class DiscoveryItem : public Item {
	Q_OBJECT

public:
	DiscoveryItem (Discovery::DnsPeer * dns_peer) : Item (dns_peer) {
		peer.username = dns_peer->get_username (); // TODO for now store username
		peer.hostname = dns_peer->get_hostname ();
		peer.port = dns_peer->get_port ();
		QHostInfo::lookupHost (peer.hostname, this, SLOT (address_lookup_complete (QHostInfo)));
	}

private slots:
	void address_lookup_complete (const QHostInfo & info) {
		auto address = Discovery::get_resolved_address (info);
		if (!address.isNull ()) {
			peer.address = address;
			edited_data (AddressField);
		}
	}
};

/* Manually added peer.
 * It is editable in place, and can be deleted by a button.
 */
class ManualItem : public Item {
	Q_OBJECT

public:
	ManualItem (QObject * parent = nullptr) : Item (parent) {}

	Qt::ItemFlags flags (int field) const Q_DECL_OVERRIDE {
		return Item::flags (field) | Qt::ItemIsEditable;
	}

	QVariant data (int field, int role) const Q_DECL_OVERRIDE {
		if (role == ButtonRole && field == UsernameField)
			return int(DeleteButton);
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

private slots:
	void address_lookup_complete (const QHostInfo & info) {
		auto address = Discovery::get_resolved_address (info);
		if (!address.isNull ()) {
			peer.address = address;
			edited_data (AddressField);
		}
	}
	void hostname_lookup_complete (const QHostInfo & info) {
		if (info.error () == QHostInfo::NoError && !info.hostName ().isEmpty ()) {
			peer.hostname = info.hostName ();
			edited_data (HostnameField);
		}
	}
};

/* Model just sets header data and dispatches button clicks.
 * Also sets drop model-wide stuff (mimetypes).
 */
class Model : public StructItemModel {
	Q_OBJECT

public:
	Model (QObject * parent = nullptr) : StructItemModel (Item::NbFields, parent) {}

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

/* Actual delegate.
 * Customize editors widgets.
 */
class InnerDelegate : public QStyledItemDelegate {
public:
	InnerDelegate (QObject * parent = nullptr) : QStyledItemDelegate (parent) {}

	QWidget * createEditor (QWidget * parent, const QStyleOptionViewItem & option,
	                        const QModelIndex & index) const Q_DECL_OVERRIDE {
		auto widget = QStyledItemDelegate::createEditor (parent, option, index);
		if (index.column () == Item::PortField) {
			// Limit port field spin box range to valid ports
			auto spin_box = qobject_cast<QSpinBox *> (widget);
			if (spin_box) {
				using L = std::numeric_limits<quint16>;
				spin_box->setMinimum (int(L::min ()));
				spin_box->setMaximum (int(L::max ()));
			}
		}
		return widget;
	}

	void updateEditorGeometry (QWidget * editor, const QStyleOptionViewItem & option,
	                           const QModelIndex &) const Q_DECL_OVERRIDE {
		editor->setGeometry (option.rect); // Force editors to fit
	}
};

/* Subclass of ButtonDelegate, sets the possible buttons.
 */
class Delegate : public ButtonDelegate {
public:
	Delegate (QObject * parent = nullptr) : ButtonDelegate (parent) {
		set_inner_delegate (new InnerDelegate (this));
		supported_buttons << SupportedButton{Item::DeleteButton, Icon::delete_peer ()};
	}
};

/* View, setup style and link to delegate.
 */
class View : public QTreeView {
private:
	Delegate * delegate{nullptr};

public:
	View (QWidget * parent = nullptr) : QTreeView (parent) {
		setAlternatingRowColors (true);
		setRootIsDecorated (false);
		setAcceptDrops (true);
		setDropIndicatorShown (true);
		setSelectionBehavior (QAbstractItemView::SelectRows);
		setSelectionMode (QAbstractItemView::ExtendedSelection);
		setSortingEnabled (true);
		setMouseTracking (true);

		delegate = new Delegate (this);
		setItemDelegate (delegate);
	}

	void setModel (Model * model) {
		QTreeView::setModel (model);
		connect (delegate, &Delegate::button_clicked, model, &Model::button_clicked);

		auto h = header ();
		h->setStretchLastSection (false);
		h->setSectionResizeMode (QHeaderView::ResizeToContents);
		h->setSectionResizeMode (Item::UsernameField, QHeaderView::Stretch);
	}

private:
	using QTreeView::setModel;

	QModelIndex moveCursor (CursorAction action, Qt::KeyboardModifiers modifiers) Q_DECL_OVERRIDE {
		auto index = currentIndex ();
		switch (action) {
		case MoveRight:
		case MoveNext:
			return index.sibling (index.row (), index.column () + 1);
		case MoveLeft:
		case MovePrevious:
			return index.sibling (index.row (), index.column () - 1);
		default:
			return QTreeView::moveCursor (action, modifiers);
		}
	}
};
}

#endif
