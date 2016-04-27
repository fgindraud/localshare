#ifndef TRANSFER_LIST_H
#define TRANSFER_LIST_H

#include "transfer.h"
#include "style.h"

#include <QAbstractTableModel>

namespace Transfer {
enum class Field { Filename, Peer, Num };

class ListModel : public QAbstractTableModel {
	Q_OBJECT

	/* List of transfers, managed by view/model.
	 * Upload and download objects are based on Base, which interfaces with us.
	 * We only maintain a list of pointers, but we remove objects if they die.
	 */

private:
	QList<Base *> transfer_list;

public:
	ListModel (QObject * parent = nullptr) : QAbstractTableModel (parent) {
		//transfer_list << new Upload ({}, "blah", this) << new Download (new QTcpSocket (this));
	}

	// Access
	bool has_transfer (const QModelIndex & index) const {
		return index.isValid () && 0 <= index.column () && index.column () < int(Field::Num) &&
		       0 <= index.row () && index.row () < transfer_list.size ();
	}
	Base * get_transfer (const QModelIndex & index) const { return transfer_list.at (index.row ()); }

	// View interface
	int rowCount (const QModelIndex & = QModelIndex ()) const { return transfer_list.size (); }
	int columnCount (const QModelIndex & = QModelIndex ()) const { return int(Field::Num); }

	QVariant data (const QModelIndex & index, int role = Qt::DisplayRole) const {
		if (!has_transfer (index))
			return {};
		auto transfer = get_transfer (index);
		switch (Field (index.column ())) {
		case Field::Filename: {
			switch (role) {
			case Qt::DecorationRole: {
				// Show icons
				if (transfer->direction == Direction::Upload)
					return Icon::upload ();
				else
					return Icon::download ();
			} break;
			case Qt::DisplayRole:
				return transfer->get_filename ();
			default:
				break;
			}
		} break;
		case Field::Peer: {
			auto peer = transfer->get_peer ();
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

public slots:
	void add_transfer (Base * transfer) {
		connect (transfer, &QObject::destroyed, this, &ListModel::transfer_deleted);
		beginInsertRows (QModelIndex (), transfer_list.size (), transfer_list.size ());
		transfer_list << transfer;
		endInsertRows ();
	}

private slots:
	void transfer_deleted (QObject * obj) {
		auto i = transfer_list.indexOf (qobject_cast<Base *> (obj));
		if (i != -1) {
			beginRemoveRows (QModelIndex (), i, i);
			transfer_list.removeAt (i);
			endRemoveRows ();
		} else {
			qCritical () << "Transfer::ListModel: transfer_deleted: cannot find transfer";
		}
	}

private:
};
}

#endif
