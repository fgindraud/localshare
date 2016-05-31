#pragma once
#ifndef CLI_TRANSFER_H
#define CLI_TRANSFER_H

#include <QHostAddress>
#include <QHostInfo>
#include <QTextStream>
#include <cstdio>

#include "cli_main.h"
#include "core_discovery.h"
#include "core_localshare.h"
#include "core_server.h"
#include "core_settings.h"
#include "core_transfer.h"

namespace Cli {
/* Both upload and download represent an event like but linear flow.
 * These classes are built on the stack before event loop start.
 * To avoid out-of-event-loop problems, defer operations in start().
 */

/* Upload.
 * LocalDnsPeer is required by Browser to filter our own ServiceRecord.
 * However in this case we have no ServiceRecord and want no filtering.
 * A default LocalDnsPeer will make Browser filter on username "", which should be ok.
 */
class Upload : public QObject {
	Q_OBJECT

private:
	const QString file_path;

	Discovery::LocalDnsPeer local_peer;
	Discovery::Browser * browser{nullptr};
	Transfer::Upload upload;

	bool peer_found{false};
	quint16 port{0};

public:
	Upload (const QString & file_path, const QString & peer_username, const QString & local_username)
	    : file_path (file_path), upload (peer_username, local_username) {}

public slots:
	void start (void) {
		browser = new Discovery::Browser (&local_peer);
		connect (browser, &Discovery::Browser::added, this, &Upload::peer_discovered);
		connect (browser, &Discovery::Browser::being_destroyed, this, &Upload::browser_end);

		verbose_print (QString ("Waiting for username \"%1\"...\n").arg (upload.get_peer_username ()));

		connect (&upload, &Transfer::Upload::failed, this, &Upload::upload_failed);
		connect (&upload, &Transfer::Upload::status_changed, this, &Upload::upload_status_changed);
		if (!upload.set_payload (file_path))
			return;

		auto & payload = upload.get_payload ();
		verbose_print (QString ("Upload payload is %1 (%2 files, total size=%3).\n")
		                   .arg (payload.get_payload_dir_display ())
		                   .arg (payload.get_nb_files ())
		                   .arg (size_to_string (payload.get_total_size ())));
	}

private slots:
	void browser_end (const QString & error) {
		if (!error.isEmpty ())
			error_print (QString ("Zeroconf browsing failed: %1\n").arg (error));
	}
	void upload_failed (void) {
		error_print (QString ("Upload failed: %1\n").arg (upload.get_error ()));
	}

	void peer_discovered (Discovery::DnsPeer * peer) {
		if (!peer_found && peer->get_username () == upload.get_peer_username ()) {
			peer_found = true;
			verbose_print (QString ("Found peer \"%1\" (\"%2\", %3:%4).\n")
			                   .arg (peer->get_username ())
			                   .arg (peer->get_service_name ())
			                   .arg (peer->get_hostname ())
			                   .arg (peer->get_port ()));
			port = peer->get_port ();
			QHostInfo::lookupHost (peer->get_hostname (), this, SLOT (peer_address_found (QHostInfo)));
			browser->deleteLater (); // No needed anymore
		} else {
			peer->deleteLater (); // Not needed
		}
	}
	void peer_address_found (const QHostInfo & info) {
		auto address = Discovery::get_resolved_address (info);
		if (address.isNull ()) {
			error_print (
			    QString ("Failed to resolve address of hostname \"%1\".\n").arg (info.hostName ()));
		} else {
			verbose_print (QString ("Connecting to %1:%2...\n").arg (address.toString ()).arg (port));
			upload.connect (address, port);
		}
	}
	void upload_status_changed (Transfer::Upload::Status new_status) {
		using Status = Transfer::Upload::Status;
		switch (new_status) {
		case Status::Transfering:
			verbose_print ("Transfer started.\n");
			break;
		case Status::Completed:
			verbose_print ("Transfer complete.\n");
			exit_nicely ();
			break;
		case Status::Rejected:
			normal_print ("Transfer rejected.\n");
			exit_error ();
			break;
		default:
			break;
		}
	}
};

/* Download is currently one shot : receive a download and quit.
 * All other downloads will be rejected:
 * - filtered downloads
 * - downloads that arrive after the first one
 * TODO receive one by one ?
 *
 * The chosen download is stored in "download".
 */
class Download : public QObject {
	Q_OBJECT

private:
	const QString target_dir;
	const QString peer_filter;
	const bool auto_accept;

	Discovery::LocalDnsPeer local_peer;
	Transfer::Server * server{nullptr};
	Discovery::ServiceRecord * service_record{nullptr};
	Transfer::Download * download{nullptr};

public:
	Download (const QString & local_username, const QString & target_dir, const QString & peer_filter,
	          bool auto_accept)
	    : target_dir (target_dir), peer_filter (peer_filter), auto_accept (auto_accept) {
		local_peer.set_requested_username (local_username);
	}

public slots:
	void start (void) {
		server = new Transfer::Server (this);
		connect (server, &Transfer::Server::download_ready, this, &Download::new_download);

		local_peer.set_port (server->port ());

		connect (&local_peer, &Discovery::LocalDnsPeer::service_name_changed, [this] {
			if (!local_peer.get_service_name ().isEmpty ())
				verbose_print (QString ("Registered as \"%1\" (\"%2\", port %3).\n")
				                   .arg (local_peer.get_username ())
				                   .arg (local_peer.get_service_name ())
				                   .arg (local_peer.get_port ()));
		});

		service_record = new Discovery::ServiceRecord (&local_peer);
		connect (service_record, &Discovery::ServiceRecord::being_destroyed, this,
		         &Download::service_record_end);
	}

private slots:
	void service_record_end (const QString & error) {
		if (!error.isEmpty ())
			error_print (QString ("Zeroconf registration failed: %1\n").arg (error));
	}
	void download_failed (void) {
		Q_ASSERT (download);
		error_print (QString ("Download failed: %1\n").arg (download->get_error ()));
	}
	void new_download (Transfer::Download * new_download) {
		Q_ASSERT (new_download->get_status () == Transfer::Download::WaitingForUserChoice);
		new_download->setParent (this);

		if (select_download (new_download)) {
			download = new_download;

			connect (download, &Transfer::Download::failed, this, &Download::download_failed);
			connect (download, &Transfer::Download::status_changed, this,
			         &Download::download_status_changed);
			download->set_target_dir (target_dir);

			// Prompt user
			if (auto_accept || prompt_user ()) {
				download->give_user_choice (Transfer::Download::Accept);
			} else {
				download->give_user_choice (Transfer::Download::Reject);
			}
			// Not needed anymore
			service_record->deleteLater ();
			server->deleteLater ();
		} else {
			connect (new_download, &Transfer::Download::failed, this, &Download::other_download_failed);
			connect (new_download, &Transfer::Download::status_changed, this,
			         &Download::other_download_status_changed);
			new_download->give_user_choice (Transfer::Download::Reject);
		}
	}
	void download_status_changed (Transfer::Download::Status new_status) {
		using Status = Transfer::Download::Status;
		switch (new_status) {
		case Status::Transfering:
			verbose_print ("Transfer started.\n");
			break;
		case Status::Completed:
			verbose_print ("Transfer complete.\n");
			exit_nicely ();
			break;
		case Status::Rejected:
			normal_print ("Transfer rejected.\n");
			exit_error ();
			break;
		default:
			break;
		}
	}

	// Ignored downloads: reject and delete them
	void other_download_failed (void) {
		auto d = qobject_cast<Transfer::Download *> (sender ());
		Q_ASSERT (d);
		qWarning ("Download[%p] failed: %s", d, qPrintable (d->get_error ()));
		d->deleteLater ();
	}
	void other_download_status_changed (Transfer::Download::Status new_status) {
		if (new_status == Transfer::Download::Rejected)
			sender ()->deleteLater ();
	}

private:
	bool select_download (const Transfer::Download * d) const {
		if (download != nullptr)
			return false; // Discard if we already have one
		if (!peer_filter.isEmpty () && peer_filter != d->get_peer_username ())
			return false;
		return true;
	}

	bool prompt_user (void) const {
		Q_ASSERT (download);
		Q_ASSERT (download->get_status () == Transfer::Download::WaitingForUserChoice);
		auto & payload = download->get_payload ();
		normal_print (QString ("Download offer from \"%1\" (%2):\n%3 (%4 files, total "
		                       "size=%5).\n")
		                  .arg (download->get_peer_username ())
		                  .arg (download->get_connection_info ())
		                  .arg (payload.get_payload_dir_display ())
		                  .arg (payload.get_nb_files ())
		                  .arg (size_to_string (payload.get_total_size ())));
		normal_print ("Accept ? y(es)/n(o)/i(nspect files) ");
		QString line = QTextStream (stdin).readLine ().trimmed ().toLower ();
		if (line.startsWith ('i')) {
			normal_print (payload.inspect_files ());
			normal_print ("Accept ? y(es)/n(o) ");
			line = QTextStream (stdin).readLine ().trimmed ().toLower ();
		}
		return line.startsWith ('y');
	}
};
}

#endif
