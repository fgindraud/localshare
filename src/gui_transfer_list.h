/* Localshare - Small file sharing application for the local network.
 * Copyright (C) 2016 Francois Gindraud
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef TRANSFER_LIST_H
#define TRANSFER_LIST_H

#include <QApplication>
#include <QFlags>
#include <QHeaderView>
#include <QStyle>
#include <QStyleOptionProgressBar>
#include <QStyledItemDelegate>
#include <QTreeView>

#include "core_localshare.h"
#include "core_transfer.h"
#include "gui_button_delegate.h"
#include "gui_struct_item_model.h"
#include "gui_style.h"

namespace Gui {
namespace TransferList {

	/* Base transfer item class, subclassed by upload of download.
	 *
	 * It will take ownership of a transfer object.
	 * This object is used as a Transfer::Base to show common stuff.
	 * It also manages the delete button.
	 */
	class Item : public StructItem {
		Q_OBJECT

	public:
		// Fields that are supported
		enum Field {
			FilenameField,
			PeerField,
			SizeField,
			ProgressField,
			RateField,
			StatusField,
			NbFields
		};

		// Buttons (see src/button_delegate.h)
		enum Role { ButtonRole = ButtonDelegate::ButtonRole };
		enum Button {
			NoButton = 0x0,
			AcceptButton = 0x1 << 0,
			CancelButton = 0x1 << 1,
			ChangeDownloadPathButton = 0x1 << 2,
			DeleteButton = 0x1 << 3
		};
		Q_DECLARE_FLAGS (Buttons, Button);

	private:
		QString rate;
		Transfer::Base * base;
		const Payload::Manager & payload;

	public:
		Item (Transfer::Base * transfer, QObject * parent = nullptr)
		    : StructItem (NbFields, parent), base (transfer), payload (transfer->get_payload ()) {
			base->setParent (this);
			connect (base->get_notifier (), &Transfer::Notifier::instant_rate, this, &Item::set_rate);
			connect (base->get_notifier (), &Transfer::Notifier::progressed, this, &Item::progressed);
		}

		QVariant data (int field, int role) const Q_DECL_OVERRIDE {
			switch (field) {
			case FilenameField: {
				// Transfer name, (in subclass:) local path, transfer type icon.
				switch (role) {
				case Qt::DisplayRole:
					return payload.get_payload_name ();
				}
			} break;
			case PeerField: {
				// Peer username, network info.
				switch (role) {
				case Qt::DisplayRole:
					return base->get_peer_username ();
				case Qt::StatusTipRole:
				case Qt::ToolTipRole:
					return base->get_connection_info ();
				}
			} break;
			case SizeField: {
				// Transfer size (human readable and detailed).
				switch (role) {
				case Qt::DisplayRole:
					return size_to_string (payload.get_total_size ());
				case Qt::StatusTipRole:
				case Qt::ToolTipRole:
					return tr ("%1B in %2 files")
					    .arg (payload.get_total_size ())
					    .arg (payload.get_nb_files ());
				}
			} break;
			case ProgressField: {
				// Progress bar, and details.
				switch (role) {
				case Qt::DisplayRole:
					return int((100 * payload.get_total_transfered_size ()) / payload.get_total_size ());
				case Qt::StatusTipRole:
				case Qt::ToolTipRole:
					return tr ("%1/%2 (files: %3/%4)")
					    .arg (size_to_string (payload.get_total_transfered_size ()),
					          size_to_string (payload.get_total_size ()))
					    .arg (payload.get_nb_files_transfered ())
					    .arg (payload.get_nb_files ());
				}
			} break;
			case RateField: {
				// Instant rate, then average rate.
				if (role == Qt::DisplayRole)
					return rate;
			} break;
			case StatusField: {
				if (role == Item::ButtonRole)
					return int(Item::DeleteButton); // all items have a delete button
			} break;
			}
			return {};
		}

		QVariant compare_data (int field) const Q_DECL_OVERRIDE {
			switch (field) {
			case SizeField:
				return payload.get_total_size ();
			default:
				return StructItem::compare_data (field);
			}
		}

		virtual bool button_clicked (int, Button btn) {
			// Return true if event has been handled to stop further handling
			if (btn == Item::DeleteButton) {
				deleteLater ();
				return true;
			}
			return false;
		}

	protected slots:
		void set_rate (qint64 new_rate_bps) {
			rate = tr ("%1/s").arg (size_to_string (new_rate_bps));
			emit data_changed (RateField, RateField, QVector<int>{Qt::DisplayRole});
		}
		void progressed (void) {
			emit data_changed (ProgressField, ProgressField,
			                   QVector<int>{Qt::DisplayRole, Qt::StatusTipRole, Qt::ToolTipRole});
		}
	};
	Q_DECLARE_OPERATORS_FOR_FLAGS (Item::Buttons);

	/* Transfer list model.
	 * Adds headers and dispatch button_clicked.
	 */
	class Model : public StructItemModel {
		Q_OBJECT

	public:
		Model (QObject * parent = nullptr) : StructItemModel (Item::NbFields, parent) {}

		QVariant headerData (int section, Qt::Orientation orientation,
		                     int role = Qt::DisplayRole) const {
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
			case Item::RateField:
				return tr ("Rate");
			case Item::StatusField:
				return tr ("Status");
			default:
				return {};
			}
		}

	public slots:
		void button_clicked (const QModelIndex & index, int btn) {
			if (has_item (index))
				get_item_t<Item *> (index)->button_clicked (index.column (), Item::Button (btn));
		}
	};

	/* Paints a progressbar for the Progress field.
	 */
	class ProgressBarDelegate : public QStyledItemDelegate {
	public:
		ProgressBarDelegate (QObject * parent = nullptr) : QStyledItemDelegate (parent) {}

		void paint (QPainter * painter, const QStyleOptionViewItem & option,
		            const QModelIndex & index) const Q_DECL_OVERRIDE {
			switch (index.column ()) {
			case Item::ProgressField: {
				// Draw a progress bar
				QStyleOptionProgressBar opt;
				init_progress_bar_style (opt, option, index);
				QApplication::style ()->drawControl (QStyle::CE_ProgressBar, &opt, painter);
			} break;
			default:
				QStyledItemDelegate::paint (painter, option, index);
			}
		}

		QSize sizeHint (const QStyleOptionViewItem & option,
		                const QModelIndex & index) const Q_DECL_OVERRIDE {
			switch (index.column ()) {
			case Item::ProgressField: {
				QStyleOptionProgressBar opt;
				init_progress_bar_style (opt, option, index);
				return QApplication::style ()->sizeFromContents (QStyle::CT_ProgressBar, &opt, QSize ());
			} break;
			default:
				return QStyledItemDelegate::sizeHint (option, index);
			}
		}

	private:
		void init_progress_bar_style (QStyleOptionProgressBar & option, const QStyleOption & from,
		                              const QModelIndex & index) const {
			option.QStyleOption::operator= (from); // Take palette, ..., AND rect
			option.minimum = 0;
			option.maximum = 100;
			option.progress = -1;
			auto value = index.data (Qt::DisplayRole);
			if (value.canConvert<int> ()) {
				auto v = value.toInt ();
				option.progress = v;
				option.text = QStringLiteral ("%1%").arg (v);
				option.textVisible = true;
			}
		}
	};

	/* Setup the ButtonDelegate buttons.
	 */
	class Delegate : public ButtonDelegate {
	public:
		Delegate (QObject * parent = nullptr) : ButtonDelegate (parent) {
			set_inner_delegate (new ProgressBarDelegate (this));

			// Setup our specific buttons
			supported_buttons << SupportedButton{Item::AcceptButton, Icon::accept ()}
			                  << SupportedButton{Item::CancelButton, Icon::cancel ()}
			                  << SupportedButton{Item::ChangeDownloadPathButton,
			                                     Icon::change_download_path ()}
			                  << SupportedButton{Item::DeleteButton, Icon::delete_transfer ()};
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
			setSelectionBehavior (QAbstractItemView::SelectRows);
			setSelectionMode (QAbstractItemView::NoSelection);
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
			h->setSectionResizeMode (Item::ProgressField, QHeaderView::Stretch);
		}

	private:
		using QTreeView::setModel;
	};
}
}

#endif
