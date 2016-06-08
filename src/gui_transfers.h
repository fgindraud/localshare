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
#ifndef GUI_TRANSFER_H
#define GUI_TRANSFER_H

#include <QFileDialog>

#include "core_settings.h"
#include "core_transfer.h"
#include "gui_style.h"
#include "gui_transfer_list.h"

namespace Gui {
namespace TransferList {

	/* Both download and upload objects will be owned by the window class.
	 *
	 * Each download/upload class mirrors the Transfer::* class that implements the protocol.
	 * They do not need to catch the signal failed() as it emits a status_changed(Error) anyway.
	 */

	/* Upload class.
	 * Created before metadata is set (call of set_payload).
	 * But only added to transfer list after that, so everything is constant (except status).
	 */
	class Upload : public Item {
		Q_OBJECT

	private:
		using Status = Transfer::Upload::Status;
		Transfer::Upload * upload;

	public:
		Upload (Transfer::Upload * transfer, QObject * parent = nullptr)
		    : Item (transfer, parent), upload (transfer) {
			connect (transfer, &Transfer::Upload::status_changed, this, &Upload::status_changed);
		}

	private:
		QVariant data (int field, int role) const Q_DECL_OVERRIDE {
			switch (field) {
			case FilenameField: {
				// Complete FilenameField (icon and status)
				switch (role) {
				case Qt::StatusTipRole:
				case Qt::ToolTipRole:
					return tr ("Uploading %1").arg (upload->get_payload ().get_payload_dir_display ());
				case Qt::DecorationRole:
					return Icon::upload ();
				}
			} break;
			case StatusField: {
				// Status message
				if (role == Qt::DisplayRole) {
					switch (upload->get_status ()) {
					case Status::Error:
						return upload->get_error ();
					case Status::Init:
						return tr ("Initializing");
					case Status::Starting:
						return tr ("Connecting");
					case Status::WaitingForPeerAnswer:
						return tr ("Waiting answer");
					case Status::Transfering:
						return tr ("Transfering");
					case Status::Completed:
						return tr ("Completed in %1")
						    .arg (msec_to_string (upload->get_notifier ()->get_transfer_time ()));
					case Status::Rejected:
						return tr ("Rejected by peer");
					}
				}
			} break;
			}
			return Item::data (field, role);
		}

		QVariant compare_data (int field) const Q_DECL_OVERRIDE {
			switch (field) {
			case StatusField:
				return int(upload->get_status ());
			default:
				return Item::compare_data (field);
			};
		}

	private slots:
		void status_changed (Status new_status) {
			if (new_status == Status::Completed) {
				// Replace instant rate by average
				set_rate (upload->get_notifier ()->get_average_rate ());
			}
			emit data_changed (StatusField, StatusField, QVector<int>{Qt::DisplayRole});
		}
	};

	/* Download.
	 * When created, metadata has already been received and will not change.
	 * TODO open button
	 */
	class Download : public Item {
		Q_OBJECT

	private:
		using Status = Transfer::Download::Status;
		Transfer::Download * download;

	public:
		Download (Transfer::Download * transfer, QObject * parent = nullptr)
		    : Item (transfer, parent), download (transfer) {
			transfer->set_target_dir (Settings::DownloadPath ().get ());
			connect (transfer, &Transfer::Download::status_changed, this, &Download::status_changed);
			if (Settings::DownloadAuto ().get ())
				transfer->give_user_choice (Transfer::Download::Accept);
		}

	private:
		QVariant data (int field, int role) const Q_DECL_OVERRIDE {
			switch (field) {
			case FilenameField: {
				// Complete FilenameField (icon, status, button)
				switch (role) {
				case Qt::StatusTipRole:
				case Qt::ToolTipRole:
					return tr ("Downloading %1 to %2")
					    .arg (download->get_payload ().get_payload_name (),
					          download->get_payload ().get_payload_dir_display ());
				case Qt::DecorationRole:
					return Icon::download ();
				case Item::ButtonRole:
					if (download->get_status () == Status::WaitingForUserChoice)
						return int(Item::ChangeDownloadPathButton);
					break;
				}
			} break;
			case StatusField: {
				// Status message
				switch (role) {
				case Qt::DisplayRole: {
					switch (download->get_status ()) {
					case Status::Error:
						return download->get_error ();
					case Status::Starting:
					case Status::WaitingForOffer:
						Q_UNREACHABLE (); // Server gives us download objects in WaitingUserChoice
						break;
					case Status::WaitingForUserChoice:
						return tr ("Accept ?");
					case Status::Transfering:
						return tr ("Transfering");
					case Status::Completed:
						return tr ("Completed in %1")
						    .arg (msec_to_string (download->get_notifier ()->get_transfer_time ()));
					case Status::Rejected:
						return tr ("Rejected");
					}
				} break;
				case Item::ButtonRole: {
					auto btns = Item::Buttons (Item::data (field, role).toInt ());
					if (download->get_status () == Status::WaitingForUserChoice)
						btns |= Item::AcceptButton | Item::CancelButton;
					return int(btns);
				} break;
				}
			} break;
			}
			return Item::data (field, role);
		}

		QVariant compare_data (int field) const Q_DECL_OVERRIDE {
			switch (field) {
			case StatusField:
				return int(download->get_status ());
			default:
				return Item::compare_data (field);
			};
		}

		bool button_clicked (int field, Button btn) Q_DECL_OVERRIDE {
			switch (download->get_status ()) {
			case Status::WaitingForUserChoice: {
				switch (btn) {
				case AcceptButton:
					download->give_user_choice (Transfer::Download::Accept);
					return true;
				case CancelButton:
					download->give_user_choice (Transfer::Download::Reject);
					return true;
				case ChangeDownloadPathButton: {
					// This button change the destination directory (like the default download path)
					QString path = QFileDialog::getExistingDirectory (
					    nullptr, tr ("Select download destination directory"),
					    download->get_payload ().get_root_dir ().path ());
					if (!path.isEmpty ()) {
						download->set_target_dir (path);
						emit data_changed (FilenameField, FilenameField,
						                   QVector<int>{Qt::StatusTipRole, Qt::ToolTipRole});
					}
					return true;
				}
				default:
					break;
				}
			} break;
			default:
				break;
			}
			return Item::button_clicked (field, btn);
		}

	private slots:
		void status_changed (Status new_status, Status old) {
			if (new_status == Status::Completed) {
				// Replace instant rate by average
				set_rate (download->get_notifier ()->get_average_rate ());
			}
			if (old == Status::WaitingForUserChoice) {
				// Clean buttons
				emit data_changed (FilenameField, FilenameField, QVector<int>{Item::ButtonRole});
				emit data_changed (StatusField, StatusField, QVector<int>{Item::ButtonRole});
			}
			emit data_changed (StatusField, StatusField, QVector<int>{Qt::DisplayRole});
		}
	};
}
}

#endif
