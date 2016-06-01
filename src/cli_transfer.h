#pragma once
#ifndef CLI_TRANSFER_H
#define CLI_TRANSFER_H

#include <QCoreApplication>
#include <QHostAddress>
#include <QHostInfo>
#include <QTextStream>
#include <QTime>
#include <cstdio>

#include "cli_indicator.h"
#include "cli_main.h"
#include "core_discovery.h"
#include "core_localshare.h"
#include "core_payload.h"
#include "core_server.h"
#include "core_settings.h"
#include "core_transfer.h"

namespace Cli {

/* Class that uses the Indicator cli gui elements to show the progress.
 */
class ProgressIndicator : public QObject, public Indicator::Container {
	Q_OBJECT

private:
	Indicator::FixedString file{tr ("File")};
	Indicator::ProgressNumber file_nb;
	Indicator::Container file_progress{" "};

	Indicator::ByteRate instant_rate;

	Indicator::ProgressBar byte_progress_bar;
	Indicator::Percent byte_progress;

public:
	ProgressIndicator (Transfer::Notifier * notifier)
	    : QObject (notifier),
	      Indicator::Container (" "),
	      file_nb (notifier->payload.get_nb_files ()) {
		file_progress.append (file).append (file_nb);
		if (notifier->payload.get_nb_files () > 1)
			append (file_progress, 1);
		append (instant_rate, 2);
		append (byte_progress_bar, 0);
		append (byte_progress, 3);

		connect (notifier, &Transfer::Notifier::progressed, this, &ProgressIndicator::update_progress);
		connect (notifier, &Transfer::Notifier::instant_rate, this, &ProgressIndicator::update_rate);
	}
public slots:
	void update_progress (void) {
		auto notifier = qobject_cast<const Transfer::Notifier *> (sender ());
		Q_ASSERT (notifier);
		auto & p = notifier->payload;
		file_nb.current = p.get_nb_files_transfered ();
		auto progress = static_cast<qreal> (p.get_total_transfered_size ()) /
		                static_cast<qreal> (qMax (p.get_total_size (), qint64 (1)));
		byte_progress_bar.set_ratio (progress);
		byte_progress.value = progress;
		draw_progress_indicator (*this);
	}
	void update_rate (qint64 bytes_per_second) { instant_rate.current = bytes_per_second; }
};

// Helper for status changed
template <typename Status>
void status_changed_helper (Status new_status, const Transfer::Notifier * notifier) {
	auto tr = [](const char * str) { return qApp->translate ("status_changed_helper", str); };
	switch (new_status) {
	case Status::Transfering: {
		verbose_print (tr ("Transfer started.\n"));
	} break;
	case Status::Completed: {
		auto elapsed = QTime (0, 0, 0).addMSecs (notifier->get_transfer_time ());
		verbose_print (tr ("Transfer complete (%1 at %2/s in %3).\n")
		                   .arg (size_to_string (notifier->payload.get_total_size ()),
		                         size_to_string (notifier->get_average_rate ()), elapsed.toString ()));
		exit_nicely ();
	} break;
	case Status::Rejected: {
		normal_print (tr ("Transfer rejected.\n"));
		exit_error ();
	} break;
	default:
		break;
	}
}

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
		connect (&upload, &Transfer::Upload::failed, this, &Upload::upload_failed);
		connect (&upload, &Transfer::Upload::status_changed, this, &Upload::upload_status_changed);

		if (!upload.set_payload (file_path))
			return;
		new ProgressIndicator (upload.get_notifier ());

		auto & payload = upload.get_payload ();
		verbose_print (tr ("Upload payload is %1 (%2 files, total size=%3).\n")
		                   .arg (payload.get_payload_dir_display (),
		                         QString::number (payload.get_nb_files ()),
		                         size_to_string (payload.get_total_size ())));

		browser = new Discovery::Browser (&local_peer);
		connect (browser, &Discovery::Browser::added, this, &Upload::peer_discovered);
		connect (browser, &Discovery::Browser::being_destroyed, this, &Upload::browser_end);

		verbose_print (tr ("Waiting for username \"%1\"...\n").arg (upload.get_peer_username ()));
	}

private slots:
	void browser_end (const QString & error) {
		if (!error.isEmpty ())
			error_print (tr ("Zeroconf browsing failed: %1\n").arg (error));
	}
	void upload_failed (void) { error_print (tr ("Upload failed: %1\n").arg (upload.get_error ())); }

	void peer_discovered (Discovery::DnsPeer * peer) {
		if (!peer_found && peer->get_username () == upload.get_peer_username ()) {
			peer_found = true;
			verbose_print (tr ("Found peer \"%1\" (\"%2\", %3:%4).\n")
			                   .arg (peer->get_username (), peer->get_service_name (),
			                         peer->get_hostname (), QString::number (peer->get_port ())));
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
			error_print (tr ("Failed to resolve address of hostname \"%1\".\n").arg (info.hostName ()));
		} else {
			verbose_print (
			    tr ("Connecting to %1:%2...\n").arg (address.toString (), QString::number (port)));
			upload.connect (address, port);
		}
	}
	void upload_status_changed (Transfer::Upload::Status new_status) const {
		status_changed_helper (new_status, upload.get_notifier ());
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
				verbose_print (tr ("Registered as \"%1\" (\"%2\", port %3).\n")
				                   .arg (local_peer.get_username (), local_peer.get_service_name (),
				                         QString::number (local_peer.get_port ())));
		});

		service_record = new Discovery::ServiceRecord (&local_peer);
		connect (service_record, &Discovery::ServiceRecord::being_destroyed, this,
		         &Download::service_record_end);
	}

private slots:
	void service_record_end (const QString & error) {
		if (!error.isEmpty ())
			error_print (tr ("Zeroconf registration failed: %1\n").arg (error));
	}
	void download_failed (void) {
		Q_ASSERT (download);
		error_print (tr ("Download failed: %1\n").arg (download->get_error ()));
	}
	void new_download (Transfer::Download * new_download) {
		Q_ASSERT (new_download->get_status () == Transfer::Download::WaitingForUserChoice);
		new_download->setParent (this);

		if (select_download (new_download)) {
			download = new_download;

			connect (download, &Transfer::Download::failed, this, &Download::download_failed);
			connect (download, &Transfer::Download::status_changed, this,
			         &Download::download_status_changed);
			new ProgressIndicator (download->get_notifier ());
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
	void download_status_changed (Transfer::Download::Status new_status) const {
		Q_ASSERT (download);
		status_changed_helper (new_status, download->get_notifier ());
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
		normal_print (tr ("Download offer from \"%1\" (%2):\n%3 (%4 files, total "
		                  "size=%5).\n")
		                  .arg (download->get_peer_username (), download->get_connection_info (),
		                        payload.get_payload_dir_display (),
		                        QString::number (payload.get_nb_files ()),
		                        size_to_string (payload.get_total_size ())));
		normal_print (tr ("Accept ? y(es)/n(o)/i(nspect files) "));
		QString line = QTextStream (stdin).readLine ().trimmed ().toLower ();
		if (line.startsWith ('i')) {
			normal_print (payload.inspect_files ()); // TODO split with "next" prompts for large values
			normal_print (tr ("Accept ? y(es)/n(o) "));
			line = QTextStream (stdin).readLine ().trimmed ().toLower ();
		}
		return line.startsWith ('y');
	}
};
}

#endif
