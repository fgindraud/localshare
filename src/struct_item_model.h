#ifndef STRUCT_ITEM_MODEL_H
#define STRUCT_ITEM_MODEL_H

#include <QAbstractItemModel>
#include <QPersistentModelIndex>
#include <algorithm>

class StructItem : public QObject {
	Q_OBJECT

	/* StructItem represent a viewable struct, of size elements.
	 * It is equivalent to a row of QAbstractItemModel.
	 * Most functions of QAbstractItemModel have equivalents in this class.
	 * However they only take the column index as position.
	 * Column index is always contained in [0, size[.
	 * Default values from QAbstractItemModel are used whenever possible.
	 *
	 * compare_data is used by sort to compare StructItems for sort.
	 *
	 * Drop setup:
	 * - set drop flag.
	 * - canDropMimeData can restrict drop by checking mimetype/action.
	 * - dropMimeData performs the drop.
	 */
public:
	const int size;

signals:
	// Equivalent to QObject::destroyed
	// useful as dynamic_cast QObject -> StructItem fails during destruction
	// so catching QObject::destroyed is not sufficient
	void being_destroyed (StructItem * item);

	// Emit when data has changed in columns [from, to] or [from, from] if to=-1
	void data_changed (int from, int to = -1, const QVector<int> & roles = QVector<int> ());

public:
	StructItem (int size, QObject * parent = nullptr) : QObject (parent), size (size) {}
	~StructItem () { emit being_destroyed (this); }

public slots:
	void deleteLater (void) {
		emit being_destroyed (this);
		QObject::deleteLater ();
	}

public:
	// Read only interface
	virtual Qt::ItemFlags flags (int /*field*/) const {
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	}
	virtual QVariant data (int field, int role) const = 0;

	// Drag & Drop
	virtual bool canDropMimeData (const QMimeData * /*mimedata*/, Qt::DropAction /*action*/,
	                              int /*field*/) const {
		return true; // No specific check
	}
	virtual bool dropMimeData (const QMimeData * /*mimedata*/, Qt::DropAction /*action*/,
	                           int /*field*/) {
		return false; // No drop
	}

	// Sort interface
	virtual QVariant compare_data (int field) const {
		return data (field, Qt::DisplayRole); // Default to text data
	}
};

class StructItemModel : public QAbstractItemModel {
	Q_OBJECT

	/* Represent a list of structs as a 2d model.
	 *
	 * We derive from QAbstractItemModel to properly support TreeViews.
	 * Valid indexes are child of root in (0,size()) x (0, struct_size).
	 *
	 * Deleting a StructItem will remove it from the model.
	 * Adding a StructItem multiple times is an error.
	 */
private:
	// StructItem are not owned
	QList<StructItem *> item_list;
	const int struct_size;

public:
	StructItemModel (int struct_size, QObject * parent = nullptr)
	    : QAbstractItemModel (parent), struct_size (struct_size) {}

	/* List interface.
	 * Similar to QList but with my formatting. Will update the views.
	 */
public:
	int size (void) const { return item_list.size (); }
	int is_empty (void) const { return item_list.isEmpty (); }

	int index_of (StructItem * item) const { return item_list.indexOf (item); }
	int index_of (QObject * obj) const {
		auto st = qobject_cast<StructItem *> (obj);
		Q_ASSERT (st != nullptr);
		return index_of (st);
	}

	StructItem * at (int i) const {
		Q_ASSERT (0 <= i && i < size ());
		return item_list.at (i);
	}
	template <typename T> T at_t (int i) const {
		auto p = qobject_cast<T> (at (i));
		Q_ASSERT (p != nullptr);
		return p;
	}

	void insert (int i, StructItem * item) {
		Q_ASSERT (item != nullptr);
		Q_ASSERT (item->size == struct_size);
		i = qBound (0, i, size ());
		beginInsertRows (QModelIndex (), i, i);
		item_list.insert (i, item);
		endInsertRows ();
		// Signals
		connect (item, &StructItem::being_destroyed, this, &StructItemModel::remove_deleted_struct);
		connect (item, &StructItem::data_changed, this, &StructItemModel::struct_data_changed);
	}
	void prepend (StructItem * item) { insert (0, item); }
	void append (StructItem * item) { insert (size (), item); }
	void remove_at (int i) {
		Q_ASSERT (0 <= i && i < size ());
		beginRemoveRows (QModelIndex (), i, i);
		auto item = item_list.takeAt (i);
		endRemoveRows ();
		// Signals
		disconnect (item, &StructItem::being_destroyed, this, &StructItemModel::remove_deleted_struct);
		disconnect (item, &StructItem::data_changed, this, &StructItemModel::struct_data_changed);
	}
private slots:
	void remove_deleted_struct (StructItem * obj) { remove_at (index_of (obj)); }

	/* List / Model interaction.
	 * List manipulation through QModelIndex structures.
	 * Forwarding of dataChanged signals.
	 */
public:
	bool has_item (const QModelIndex & index) const {
		return is_top_level (index) && index.row () < size () && index.column () < struct_size;
	}
	StructItem * get_item (const QModelIndex & index) const { return at (index.row ()); }
	template <typename T> T get_item_t (const QModelIndex & index) const {
		auto p = qobject_cast<T> (get_item (index));
		Q_ASSERT (p != nullptr);
		return p;
	}

private slots:
	void struct_data_changed (int from, int to, const QVector<int> & roles) {
		if (to == -1)
			to = from;
		auto i = index_of (sender ());
		Q_ASSERT (i != -1);
		emit dataChanged (index (i, from), index (i, to), roles); // forward
	}

	/* QModelIndex generation.
	 */
private:
	bool is_root (const QModelIndex & index) const { return index == QModelIndex (); }
	bool is_top_level (const QModelIndex & index) const {
		return index.isValid () && is_root (index.parent ());
	}

public:
	QModelIndex parent (const QModelIndex & /*index*/) const Q_DECL_OVERRIDE {
		return QModelIndex (); // No index has valid parents in our model...
	}
	QModelIndex index (int row, int column,
	                   const QModelIndex & parent = QModelIndex ()) const Q_DECL_OVERRIDE {
		if (!hasIndex (row, column, parent)) // check has rows, etc
			return QModelIndex ();
		Q_ASSERT (is_root (parent)); // only root has rows
		return createIndex (row, column);
	}

	/* Read only model interface.
	 */
public:
	Qt::ItemFlags flags (const QModelIndex & index) const Q_DECL_OVERRIDE {
		if (has_item (index))
			return get_item (index)->flags (index.column ());
		return 0;
	}
	QVariant data (const QModelIndex & index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE {
		if (has_item (index))
			return get_item (index)->data (index.column (), role);
		return {};
	}
	// headerData must be provided by subclass
	int rowCount (const QModelIndex & index = QModelIndex ()) const Q_DECL_OVERRIDE {
		if (is_root (index))
			return size ();
		return 0;
	}
	int columnCount (const QModelIndex & index = QModelIndex ()) const Q_DECL_OVERRIDE {
		if (is_root (index))
			return struct_size;
		return 0;
	}

	// Editable interface
	// TODO

	/* Resizing interface.
	 * Only moving elements is supported.
	 */
	bool moveRows (const QModelIndex & src_parent, int row, int count, const QModelIndex & dst_parent,
	               int pos) Q_DECL_OVERRIDE {
		if (!(is_root (src_parent) && is_root (dst_parent)))
			return false;
		if (!(0 <= row && 0 < count && row + count <= size () && 0 <= pos && pos < size ()))
			return false;
		if (!beginMoveRows (src_parent, row, row + count - 1, src_parent, pos))
			return false; // Qt doc: fails if move into the interval
		// Rotate seems to be the easiest way to implement move
		if (pos < row) {
			std::rotate (item_list.begin () + pos, item_list.begin () + row,
			             item_list.begin () + row + count);
		} else { // pos >= row + count
			std::rotate (item_list.begin () + row, item_list.begin () + row + count,
			             item_list.begin () + pos);
		}
		endMoveRows ();
		return true;
	}

	/* Drag & drop interface.
	 * Supports dropping on an item (forwarded to item).
	 * Does not support the default drag system of models (uses removeRows...).
	 *
	 * To support drops:
	 * - support drops in items.
	 * - reimplement mimeTypes to indicate supported mime types.
	 * - reimplement supportedDropActions to indicate supported drop actions.
	 * Internal Drag: TODO
	 */
	bool canDropMimeData (const QMimeData * mimedata, Qt::DropAction action, int row, int column,
	                      const QModelIndex & parent) const Q_DECL_OVERRIDE {
		if (row == -1 && column == -1 && has_item (parent)) // Direct drop on item
			return get_item (parent)->canDropMimeData (mimedata, action, parent.column ());
		return false;
	}
	bool dropMimeData (const QMimeData * mimedata, Qt::DropAction action, int row, int column,
	                   const QModelIndex & parent) Q_DECL_OVERRIDE {
		if (action == Qt::IgnoreAction)
			return true;                                      // ignore
		if (row == -1 && column == -1 && has_item (parent)) // Direct drop on item
			return get_item (parent)->dropMimeData (mimedata, action, parent.column ());
		return false;
	}
	QStringList mimeTypes (void) const Q_DECL_OVERRIDE {
		return {}; // None supported
	}
	Qt::DropActions supportedDropActions (void) const Q_DECL_OVERRIDE {
		return Qt::IgnoreAction; // None supported
	}

	/* Sort interface.
	 * Supports sorting the model by column.
	 */
	void sort (int column, Qt::SortOrder order = Qt::AscendingOrder) Q_DECL_OVERRIDE {
		if (!(0 <= column && column < struct_size))
			return;
		emit layoutAboutToBeChanged (QList<QPersistentModelIndex> (),
		                             QAbstractItemModel::VerticalSortHint);
		// Save indexes and associated items (or nullptrs for invalid index)
		auto persistent_indexes = persistentIndexList ();
		QList<StructItem *> persistent_items;
		for (auto & index : persistent_indexes) {
			if (has_item (index)) {
				persistent_items.append (get_item (index));
			} else {
				persistent_items.append (nullptr);
			}
		}
		// Sort
		if (order == Qt::AscendingOrder) {
			std::sort (item_list.begin (), item_list.end (),
			           [=](const StructItem * a, const StructItem * b) {
				           return a->compare_data (column) < b->compare_data (column);
				         });
		} else {
			std::sort (item_list.begin (), item_list.end (),
			           [=](const StructItem * a, const StructItem * b) {
				           return a->compare_data (column) > b->compare_data (column);
				         });
		}
		// Correct indexes
		for (auto i = 0; i < persistent_indexes.size (); ++i) {
			if (persistent_items[i] != nullptr) {
				auto new_row = index_of (persistent_items[i]);
				Q_ASSERT (new_row != -1); // Items should not disappear
				changePersistentIndex (persistent_indexes[i],
				                       index (new_row, persistent_indexes[i].column ()));
			}
		}
		emit layoutChanged (QList<QPersistentModelIndex> (), QAbstractItemModel::VerticalSortHint);
	}
};

#endif
