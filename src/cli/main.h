#pragma once
#ifndef CONSOLE_MAIN_H
#define CONSOLE_MAIN_H

#include <QByteArray>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostAddress>
#include <QHostInfo>
#include <QTimer>
#include <QtGlobal>
#include <cstdio>

#include "core/discovery.h"
#include "core/localshare.h"
#include "core/server.h"
#include "core/settings.h"
#include "core/transfer.h"

/* Determine if we are in batch mode.
 */
inline bool is_console_mode (int argc, const char * const * argv) {
	const char * trigger_console_mode[] = {"-d",     "--download", "-u",        "--upload", "-h",
	                                       "--help", "-v",         "--version", nullptr};
	for (int i = 1; i < argc; ++i)
		for (int j = 0; trigger_console_mode[j] != nullptr; ++j)
			if (qstrncmp (argv[i], trigger_console_mode[j], qstrlen (trigger_console_mode[j])) == 0)
				return true;
	return false;
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
		browser = new Discovery::Browser (&local_peer);
		connect (browser, &Discovery::Browser::added, this, &Upload::peer_discovered);
		connect (browser, &Discovery::Browser::being_destroyed, this, &Upload::browser_end);

		fprintf (stdout, "Waiting for username \"%s\"...\n", qPrintable (upload.get_peer_username ()));

		connect (&upload, &Transfer::Upload::failed, this, &Upload::upload_failed);
		connect (&upload, &Transfer::Upload::status_changed, this, &Upload::upload_status_changed);
		if (!upload.set_payload (file_path))
			return;

		auto & payload = upload.get_payload ();
		fprintf (stdout, "Upload payload is %s (%d files, total size=%s).\n",
		         qPrintable (payload.get_payload_dir_display ()), payload.get_nb_files (),
		         qPrintable (size_to_string (payload.get_total_size ())));
	}

private slots:
	void browser_end (const QString & error) {
		if (!error.isEmpty ()) {
			fprintf (stderr, "Zeroconf browsing failed: %s\n", qPrintable (error));
			QCoreApplication::exit (EXIT_FAILURE);
		}
	}
	void upload_failed (void) {
		fprintf (stderr, "Upload failed: %s\n", qPrintable (upload.get_error ()));
		QCoreApplication::exit (EXIT_FAILURE);
	}

	void peer_discovered (Discovery::DnsPeer * peer) {
		if (!peer_found && peer->get_username () == upload.get_peer_username ()) {
			peer_found = true;
			fprintf (stdout, "Found peer \"%s\" (\"%s\", %s:%u).\n", qPrintable (peer->get_username ()),
			         qPrintable (peer->get_service_name ()), qPrintable (peer->get_hostname ()),
			         peer->get_port ());
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
			fprintf (stderr, "Unable to resolve address of hostname \"%s\" !\n",
			         qPrintable (info.hostName ()));
			QCoreApplication::exit (EXIT_FAILURE);
		} else {
			fprintf (stdout, "Connecting to %s:%u...\n", qPrintable (address.toString ()), port);
			upload.connect (address, port);
		}
	}
	void upload_status_changed (Transfer::Upload::Status new_status) {
		using Status = Transfer::Upload::Status;
		if (new_status == Status::Transfering)
			fprintf (stdout, "Transfer started !\n");
		if (new_status == Status::Completed) {
			fprintf (stdout, "Transfer complete !\n");
			QCoreApplication::exit (EXIT_SUCCESS);
		}
	}
};

/* Download.
 * Download is currently one shot : receive a download and quit.
 * TODO receive one by one.
 * TODO -y mode to auto accept
 */
class Download : public QObject {
	Q_OBJECT

private:
	const QString target_dir;
	const QString peer_filter;

	Transfer::Server server;
	Discovery::LocalDnsPeer local_peer;
	Transfer::Download * download{nullptr};

public:
	Download (const QString & local_username, const QString & target_dir, const QString & peer_filter)
	    : target_dir (target_dir), peer_filter (peer_filter) {
		local_peer.set_port (server.port ());
		local_peer.set_requested_username (local_username);
	}

public slots:
	void start (void) {
		connect (&local_peer, &Discovery::LocalDnsPeer::service_name_changed, [this] {
			fprintf (stdout, "Registered as \"%s\" (\"%s\", port %u).\n",
			         qPrintable (local_peer.get_username ()), qPrintable (local_peer.get_service_name ()),
			         local_peer.get_port ());
		});

		auto service_record = new Discovery::ServiceRecord (&local_peer);
		connect (service_record, &Discovery::ServiceRecord::being_destroyed, this,
		         &Download::service_record_end);

		connect (&server, &Transfer::Server::download_ready, this, &Download::new_download);
	}

private slots:
	void service_record_end (const QString & error) {
		if (!error.isEmpty ()) {
			fprintf (stderr, "Zeroconf registration failed: %s\n", qPrintable (error));
			QCoreApplication::exit (EXIT_FAILURE);
		}
	}
	void download_failed (void) {
		auto d = qobject_cast<Transfer::Download *> (sender ());
		Q_ASSERT (d);
		// TODO make rejected a completion to not print a warning
		qWarning ("Download[%p] failed: %s", d, qPrintable (d->get_error ()));
		if (d == download) {
			QCoreApplication::exit (EXIT_FAILURE);
		} else {
			d->deleteLater ();
		}
	}

	void new_download (Transfer::Download * new_download) {
		Q_ASSERT (new_download->get_status () == Transfer::Download::WaitingForUserChoice);
		new_download->setParent (this);
		connect (new_download, &Transfer::Download::failed, this, &Download::download_failed);
		if (accept_download (new_download)) {
			download = new_download;
			// Proposal to user TODO nicer...
			download->set_target_dir (target_dir);
			fprintf (stdout, "Download offer from \"%s\" (%s):\n",
			         qPrintable (download->get_peer_username ()),
			         qPrintable (download->get_connection_info ()));
			auto & payload = download->get_payload ();
			fprintf (stdout, "%s (%d files, total size=%s).\n",
			         qPrintable (payload.get_payload_dir_display ()), payload.get_nb_files (),
			         qPrintable (size_to_string (payload.get_total_size ())));
			fprintf (stdout, "Accept [Y/n] ?");
			char c = 'y';
			fscanf (stdin, "%c", &c);
			if (c == 'n') {
				download->give_user_choice (Transfer::Download::Rejected);
			} else {
				download->give_user_choice (Transfer::Download::Accepted);
				connect (download, &Transfer::Download::status_changed, this,
				         &Download::download_status_changed);
			}
			// TODO rejecting makes uploader have a QSocketNotifier warning
			// it appears during construction
			// ???
		} else {
			new_download->give_user_choice (Transfer::Download::Rejected);
		}
	}
	void download_status_changed (Transfer::Download::Status new_status) {
		using Status = Transfer::Download::Status;
		if (new_status == Status::Transfering)
			fprintf (stdout, "Transfer started !\n");
		if (new_status == Status::Completed) {
			fprintf (stdout, "Transfer complete !\n");
			QCoreApplication::exit (EXIT_SUCCESS);
		}
	}

private:
	bool accept_download (const Transfer::Download * d) const {
		if (download != nullptr)
			return false; // Discard if we already have one
		if (!peer_filter.isEmpty () && peer_filter != d->get_peer_username ())
			return false;
		return true;
	}
};

/* Handler that suppress output from debug/warnings.
 * It preserves the abort semantics of qFatal().
 */
inline void suppress_output_handler (QtMsgType type, const QMessageLogContext &, const QString &) {
	if (type == QtFatalMsg)
		abort ();
}

/* Entry point.
 * Parses command line arguments and start appropriate mode.
 */
inline int console_main (int & argc, char **& argv) {
	QCoreApplication app (argc, argv);
	Const::setup (app);

	QCommandLineParser parser;
	parser.setApplicationDescription (
	    QString ("Small file sharing application for the local network.\n"
	             "\n"
	             "No options: use graphical mode.\n"
	             "Upload and download mode are exclusive.\n"
	             "Usage example:\n"
	             "$ %1 -u <file> -p <destination_username>   # Upload\n"
	             "$ %1 -d   # Download from anyone\n")
	        .arg (Const::app_name));
	auto help_opt = parser.addHelpOption ();
	auto version_opt = parser.addVersionOption ();
	QCommandLineOption download_opt (QStringList () << "d"
	                                                << "download",
	                                 "Wait for a download.");
	parser.addOption (download_opt);
	QCommandLineOption upload_opt (QStringList () << "u"
	                                              << "upload",
	                               "Uploads a file to <peer>.", "filename");
	parser.addOption (upload_opt);
	QCommandLineOption username_opt (QStringList () << "n"
	                                                << "name",
	                                 "Local Zeroconf username", "username",
	                                 Settings::Username ().get ());
	parser.addOption (username_opt);
	QCommandLineOption peer_opt (QStringList () << "p"
	                                            << "peer",
	                             "Peer Zeroconf username.", "username");
	parser.addOption (peer_opt);
	QCommandLineOption target_dir_opt (QStringList () << "t"
	                                                  << "target-dir",
	                                   "Target directory for downloads.", "path",
	                                   Settings::DownloadPath ().get ());
	parser.addOption (target_dir_opt);
	QCommandLineOption quiet_opt (QStringList () << "q"
	                                             << "quiet",
	                              "Remove verbose debug/warning messages.");
	parser.addOption (quiet_opt);
	QCommandLineOption very_quiet_opt (QStringList () << "Q"
	                                                  << "very-quiet",
	                                   "Remove all output.");
	parser.addOption (very_quiet_opt);
	parser.process (app);

	const auto download_mode = parser.isSet (download_opt);
	const auto upload_mode = parser.isSet (upload_opt);

	// Output control
	if (parser.isSet (very_quiet_opt) && download_mode) {
		// TODO needs -y command
		fprintf (stderr, "Error: Download requires messages !\n");
		return EXIT_FAILURE;
	}
	if (parser.isSet (very_quiet_opt)) {
		fclose (stderr);
		fclose (stdout);
	}
	if (parser.isSet (quiet_opt) || parser.isSet (very_quiet_opt)) {
		qInstallMessageHandler (suppress_output_handler);
	}

	if (download_mode && upload_mode) {
		fprintf (stderr, "Error: upload and download are exclusive !\n");
		return EXIT_FAILURE;
	}
	if (!download_mode && !upload_mode) {
		fprintf (stderr, "Error: no mode set (upload or download) !\n");
		return EXIT_FAILURE;
	}

	if (upload_mode) {
		// Upload
		if (!parser.isSet (peer_opt)) {
			fprintf (stderr, "Error: upload target peer not set !\n");
			return EXIT_FAILURE;
		}
		Upload upload (parser.value (upload_opt), parser.value (peer_opt), parser.value (username_opt));
		QTimer::singleShot (0, &upload, SLOT (start ()));
		return app.exec ();
	}
	if (download_mode) {
		// Download
		Download download (parser.value (username_opt), parser.value (target_dir_opt),
		                   parser.value (peer_opt));
		QTimer::singleShot (0, &download, SLOT (start ()));
		return app.exec ();
	}
	Q_UNREACHABLE ();
	return EXIT_FAILURE;
}

#endif
