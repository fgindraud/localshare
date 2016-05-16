#ifndef TRANSFER_MODEL_H
#define TRANSFER_MODEL_H

#include <QDebug>

#include "struct_item_model.h"

namespace Transfer {

class Item : public StructItem {
	Q_OBJECT

	/* Base transfer item class.
	 * Subclassed by upload or download.
	 */
public:
	// Fields that are supported
	enum Field { FilenameField, PeerField, SizeField, ProgressField, StatusField, NbFields };

	/* Status can have buttons to interact with user (accept/abort transfer).
	 * The View/Model system doesn't support buttons very easily.
	 * The chosen approach is to have a new Role to tell which buttons are enabled.
	 * This Role stores an OR of flags, and an invalid QVariant is treated as a 0 flag.
	 * The buttons are manually painted on the view by a custom delegate.
	 * The delegate also catches click events.
	 */
	enum Role { ButtonRole = Qt::UserRole };
	enum Button {
		NoButton = 0x0,
		AcceptButton = 0x1 << 0,
		CancelButton = 0x1 << 1,
		ChangeDownloadPathButton = 0x1 << 2,
		DeleteButton = 0x1 << 3
	};
	Q_DECLARE_FLAGS (Buttons, Button);
	Q_FLAG (Buttons);

public:
	Item (QObject * parent = nullptr) : StructItem (NbFields, parent) {}

	virtual void button_clicked (int field, Button btn) = 0;
};
Q_DECLARE_OPERATORS_FOR_FLAGS (Item::Buttons);

class Model : public StructItemModel {
	Q_OBJECT

	/* Transfer list model.
	 * Adds headers and dispatch button_clicked.
	 */
public:
	Model (QObject * parent = nullptr) : StructItemModel (Item::NbFields, parent) {}

	QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const {
		if (!(role == Qt::DisplayRole && orientation == Qt::Horizontal))
			return {};
		switch (section) {
		case Item::FilenameField:
			return tr ("File");
		case Item::PeerField:
			return tr ("Peer");
		case Item::SizeField:
			return tr ("File size");
		case Item::ProgressField:
			return tr ("Transferred");
		case Item::StatusField:
			return tr ("Status");
		default:
			return {};
		}
	}

public slots:
	void button_clicked (const QModelIndex & index, Item::Button btn) {
		if (has_item (index))
			return get_item_t<Item *> (index)->button_clicked (index.column (), btn);
		qCritical () << "button_clicked on invalid item" << index << btn;
	}
};

inline QString size_to_string (qint64 size) {
	// Find correct unit to fit size.
	double num = size;
	double increment = 1024.0;
	static QString suffixes[] = {QObject::tr ("B"),
	                             QObject::tr ("KiB"),
	                             QObject::tr ("MiB"),
	                             QObject::tr ("GiB"),
	                             QObject::tr ("TiB"),
	                             QObject::tr ("PiB"),
	                             {}};
	int unit_idx = 0;
	while (num >= increment && !suffixes[unit_idx + 1].isEmpty ()) {
		unit_idx++;
		num /= increment;
	}
	return QString ().setNum (num, 'f', 2) + suffixes[unit_idx];
}
}

#endif
