#ifndef TRANSFER_MODEL_H
#define TRANSFER_MODEL_H

#include "struct_item_model.h"
#include "style.h"

#include "localshare.h"

namespace Transfer {

enum class Direction { Upload, Download };

enum class Field { Filename, Peer, Num }; // Item

class Item : public StructItem {
	Q_OBJECT

public:
	const Direction direction;

signals:
	void filename_changed (void);
	void peer_changed (void);

public:
	Item (Direction direction, QObject * parent = nullptr)
	    : StructItem (int(Field::Num), parent), direction (direction) {}

	virtual QString get_filename (void) const = 0;
	virtual Peer get_peer (void) const = 0;

	QVariant data (int elem, int role) const {
		switch (Field (elem)) {
		case Field::Filename: {
			switch (role) {
			case Qt::DecorationRole: {
				// Show icons
				if (direction == Direction::Upload)
					return Icon::upload ();
				else
					return Icon::download ();
			} break;
			case Qt::DisplayRole:
				return get_filename ();
			default:
				break;
			}
		} break;
		case Field::Peer: {
			auto peer = get_peer ();
			switch (role) {
			case Qt::DisplayRole: {
				if (peer.username.isEmpty ())
					return peer.address.toString ();
				else
					return peer.username;
			} break;
			case Qt::ToolTipRole:
				return QString ("%1:%2").arg (peer.address.toString ()).arg (peer.port);
			default:
				break;
			};
		} break;
		default:
			break;
		}
		return {};
	}
};

class ListModel : public StructItemModel {
	Q_OBJECT

public:
	ListModel (QObject * parent = nullptr) : StructItemModel (int(Field::Num), parent) {}

	QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const {
		if (!(role == Qt::DisplayRole && orientation == Qt::Horizontal))
			return {};
		switch (Field (section)) {
		case Field::Filename:
			return tr ("Filename");
		case Field::Peer:
			return tr ("Peer");
		default:
			return {};
		}
	}
};
}

#endif
