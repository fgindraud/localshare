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
#ifndef GUI_DISCOVERY_SUBSYSTEM_H
#define GUI_DISCOVERY_SUBSYSTEM_H

#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QStyle>

#include "core_discovery.h"
#include "gui_style.h"

namespace Gui {
/* Discovery subsystem.
 * It allows to dynamically manage discovery services.
 *
 * Browser and ServiceRecord are (almost) independent.
 * Browser depends on the service_record instance name.
 * This name is preset in local_peer, and may change after registration.
 * Thankfully Browser does not need to restart, as the name is only used for filtering.
 *
 * This widget acts as a Status Bar.
 * It normally displays the server port and the registration status.
 * In case of errors, it grows with a big warning icon and a restart button.
 * This button will clear errors and restart all dead services.
 * ServiceRecord and Browser will emit being_destroyed on destruction, with a possible error.
 */
class DiscoverySubSystem : public QStatusBar {
	Q_OBJECT

private:
	// Shorter names
	using LocalDnsPeer = Discovery::LocalDnsPeer;
	using ServiceRecord = Discovery::ServiceRecord;
	using Browser = Discovery::Browser;
	using SubSystem = DiscoverySubSystem;

	LocalDnsPeer * local_peer;

	ServiceRecord * service_record{nullptr};
	Browser * browser{nullptr};

	QString current_errors{"Init"}; // A non empty text is important for initialisation
	QLabel * warning_symbol{nullptr};
	QLabel * message{nullptr};
	QPushButton * restart{nullptr};

signals:
	void new_discovered_peer (Discovery::DnsPeer * peer);

public:
	DiscoverySubSystem (LocalDnsPeer * local_peer_, QWidget * parent = nullptr)
	    : QStatusBar (parent), local_peer (local_peer_) {
		// Create widgets of status bar
		warning_symbol = new QLabel (this);
		warning_symbol->setPixmap (
		    Icon::warning ().pixmap (style ()->pixelMetric (QStyle::PM_MessageBoxIconSize)));
		addWidget (warning_symbol);

		message = new QLabel (this);
		addWidget (message);

		restart = new QPushButton (tr ("Restart discovery ?"), this);
		restart->setIcon (Icon::restart_discovery ());
		addWidget (restart);
		connect (restart, &QPushButton::clicked, this, &SubSystem::start_services);

		connect (local_peer, &LocalDnsPeer::requested_service_name_changed, this,
		         &SubSystem::rename_service_record);
		connect (local_peer, &LocalDnsPeer::service_name_changed, this, &SubSystem::message_changed);

		// Start services for the first time
		start_services ();
	}

private slots:
	void start_service_record (void) {
		service_record = new ServiceRecord (local_peer);
		connect (service_record, &ServiceRecord::being_destroyed, this,
		         &SubSystem::service_record_destroyed);
	}
	void service_record_destroyed (const QString & error) {
		service_record = nullptr;
		append_error (error);
	}

	void start_browser (void) {
		browser = new Browser (local_peer);
		connect (browser, &Browser::added, this, &SubSystem::new_discovered_peer);
		connect (browser, &Browser::being_destroyed, this, &SubSystem::browser_destroyed);
	}
	void browser_destroyed (const QString & error) {
		browser = nullptr;
		append_error (error);
	}

	void start_services (void) {
		// Restart all dead services
		if (!service_record)
			start_service_record ();
		if (!browser)
			start_browser ();
		clear_errors ();
	}

	void rename_service_record (void) {
		// Kill current ServiceRecord and restart
		Q_ASSERT (service_record != nullptr);
		connect (service_record, &QObject::destroyed, this, &SubSystem::start_service_record);
		service_record->deleteLater ();
	}

	void message_changed (void) { message->setText (make_status_message () + current_errors); }

private:
	void append_error (const QString & error) {
		if (!error.isEmpty ()) {
			if (current_errors.isEmpty ()) {
				insertWidget (0, warning_symbol);
				warning_symbol->show ();
				addWidget (restart);
				restart->show ();
			}
			current_errors += "\n" + error;
		}
		message_changed ();
	}

	void clear_errors (void) {
		if (!current_errors.isEmpty ()) {
			removeWidget (warning_symbol);
			removeWidget (restart);
			current_errors.clear ();
		}
		message_changed ();
	}

	QString make_status_message (void) const {
		if (service_record) {
			if (!local_peer->get_service_name ().isEmpty ()) {
				return tr ("%1 running on port %2 and registered with username \"%3\".")
				    .arg (Const::app_display_name, QString::number (local_peer->get_port ()),
				          local_peer->get_username ());
			} else {
				return tr ("%1 running on port %2 and registering...")
				    .arg (Const::app_display_name, QString::number (local_peer->get_port ()));
			}
		} else {
			return tr ("%1 running on port %2 and unregistered !")
			    .arg (Const::app_display_name, QString::number (local_peer->get_port ()));
		}
	}
};
}

#endif
