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
#ifndef CLI_MISC_H
#define CLI_MISC_H

#include <QStringList>
#include <QTimer>

#include "cli_indicator.h"
#include "cli_main.h"
#include "core_discovery.h"

namespace Cli {

// Small class that lists the first batch of peers and quit.
class PeerBrowser : public QObject {
	Q_OBJECT

private:
	Discovery::LocalDnsPeer local_peer; // dummy

public:
	PeerBrowser () {
		auto browser = new Discovery::Browser (&local_peer);
		connect (browser, &Discovery::Browser::added, this, &PeerBrowser::new_peer);
		connect (browser, &Discovery::Browser::end_of_batch, this, &PeerBrowser::end_browsing);
		QTimer::singleShot (3 * 1000, this, SLOT (end_browsing ()));
	}

private slots:
	void new_peer (Discovery::DnsPeer * peer) {
		always_print (QString ("%1 (%2)\n").arg (peer->get_username (), peer->get_hostname ()));
	}
	void end_browsing (void) { exit_nicely (); }
};
}

#endif
