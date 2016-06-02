#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>
#include <QtGlobal>
#include <cstdio>

#include "cli_indicator.h"
#include "cli_main.h"
#include "cli_transfer.h"
#include "compatibility.h"
#include "core_transfer.h"
#include "portability.h"

namespace Cli {

namespace {
	enum Verbosity { VerboseLevel = 2, NormalLevel = 1, QuietLevel = 0 } verbosity{NormalLevel};
	bool last_output_was_progress{false};

	void print (FILE * stream, const QString & msg, Verbosity min_level) {
		if (verbosity >= min_level)
			QTextStream (stream) << msg;
	}

	void insert_newline_if_needed (void) {
		if (last_output_was_progress) {
			print (stdout, "\n", NormalLevel);
			last_output_was_progress = false;
		}
	}

	// Handler that suppress output from debug/warnings.
	QtMessageHandler old_handler{nullptr};
	void suppress_output_handler (QtMsgType type, const QMessageLogContext & context,
	                              const QString & msg) {
		if (!qt_message_is_important (type) && verbosity < VerboseLevel)
			return; // Ignore
		insert_newline_if_needed ();
		old_handler (type, context, msg);
	}
}

// Progress indicator
void draw_progress_indicator (const Indicator::Item & indicator) {
	print (stdout, indicator.draw (terminal_width ()) + '\r', NormalLevel);
	last_output_was_progress = true;
}

// Reporting and quit for inside the event loop
void verbose_print (const QString & msg) {
	insert_newline_if_needed ();
	print (stdout, msg, VerboseLevel);
}
void normal_print (const QString & msg) {
	insert_newline_if_needed ();
	print (stdout, msg, NormalLevel);
}
void error_print (const QString & msg) {
	insert_newline_if_needed ();
	print (stderr, msg, QuietLevel);
	exit_error ();
}
void exit_nicely (void) {
	QCoreApplication::exit (EXIT_SUCCESS);
}
void exit_error (void) {
	QCoreApplication::exit (EXIT_FAILURE);
}

/* Entry point.
 * Parses command line arguments and start appropriate mode.
 */
int start (int & argc, char **& argv) {
	QCoreApplication app (argc, argv);
	Const::setup (app);

	auto tr = [&](const char * text) { return app.translate ("cli_main", text); };
	QCommandLineParser parser;
	parser.setApplicationDescription (
	    tr ("Small file sharing application for the local network.\n"
	        "\n"
	        "No options: use graphical mode.\n"
	        "Upload and download mode are exclusive.\n"
	        "Returns 0 if the transfer completed correctly, 1 otherwise.\n"
	        "\n"
	        "Usage example:\n"
	        "$ %1 -u <file> -p <destination_username>   # Upload\n"
	        "$ %1 -d   # Download from anyone\n"
	        "$ %1 -d -p <peer>   # Download from <peer> only\n"
	        "$ %1 -d -n <username>   # Download as destination <username>")
	        .arg (Const::app_name));
	auto help_opt = parser.addHelpOption ();
	QCommandLineOption version_opt (QStringList () << "V"
	                                               << "version",
	                                tr ("Print the program name and version"));
	parser.addOption (version_opt);
	QCommandLineOption download_opt (QStringList () << "d"
	                                                << "download",
	                                 tr ("Wait for a download."));
	parser.addOption (download_opt);
	QCommandLineOption upload_opt (QStringList () << "u"
	                                              << "upload",
	                               tr ("Uploads a file to <peer>."), tr ("filename"));
	parser.addOption (upload_opt);
	QCommandLineOption username_opt (QStringList () << "n"
	                                                << "name",
	                                 tr ("Local Zeroconf username"), tr ("username"),
	                                 Settings::Username ().get ());
	parser.addOption (username_opt);
	QCommandLineOption peer_opt (QStringList () << "p"
	                                            << "peer",
	                             tr ("Peer Zeroconf username."), tr ("username"));
	parser.addOption (peer_opt);
	QCommandLineOption target_dir_opt (QStringList () << "t"
	                                                  << "target-dir",
	                                   tr ("Target directory for downloads."), tr ("path"),
	                                   Settings::DownloadPath ().get ());
	parser.addOption (target_dir_opt);
	QCommandLineOption yes_opt (QStringList () << "y"
	                                           << "yes",
	                            tr ("Automatically accept prompts."));
	parser.addOption (yes_opt);
	QCommandLineOption verbose_opt (QStringList () << "v"
	                                               << "verbose",
	                                tr ("Show more messages."));
	parser.addOption (verbose_opt);
	QCommandLineOption quiet_opt (QStringList () << "q"
	                                             << "quiet",
	                              tr ("Hide all output, only show errors."));
	parser.addOption (quiet_opt);
	QCommandLineOption hidden_files_opt (QStringList () << "hidden",
	                                     tr ("Send hidden files when sending directories."));
	parser.addOption (hidden_files_opt);

	parser.process (app);
	if (parser.isSet (version_opt)) {
		normal_print (QStringLiteral ("%1 %2\n").arg (Const::app_name).arg (Const::app_version));
		return EXIT_SUCCESS;
	}

	// Output control
	if (parser.isSet (verbose_opt))
		verbosity = VerboseLevel;
	if (parser.isSet (quiet_opt))
		verbosity = QuietLevel;
	old_handler = qInstallMessageHandler (suppress_output_handler);

	const auto download_mode = parser.isSet (download_opt);
	const auto upload_mode = parser.isSet (upload_opt);

	if (download_mode && upload_mode) {
		QTextStream (stderr) << tr (
		    "Error: upload and download modes are exclusive (see -h for help).\n");
		return EXIT_FAILURE;
	}
	if (!download_mode && !upload_mode) {
		QTextStream (stderr) << tr ("Error: no mode set (see -h for help).\n");
		return EXIT_FAILURE;
	}

	if (upload_mode) {
		// Upload
		if (!parser.isSet (peer_opt)) {
			QTextStream (stderr) << tr ("Error: target peer of upload is not set (see -h for help).\n");
			return EXIT_FAILURE;
		}
		Upload upload (parser.value (upload_opt), parser.value (peer_opt), parser.value (username_opt),
		               parser.isSet (hidden_files_opt));
		QTimer::singleShot (0, &upload, SLOT (start ()));
		return app.exec ();
	}
	if (download_mode) {
		// Download
		if (verbosity <= QuietLevel && !parser.isSet (yes_opt)) {
			QTextStream (stderr) << tr ("Error: download accept prompt is unavailable in --quiet mode; "
			                            "use -y to bypass it (see -h for help).\n");
			return EXIT_FAILURE;
		}
		Download download (parser.value (username_opt), parser.value (target_dir_opt),
		                   parser.value (peer_opt), parser.isSet (yes_opt));
		QTimer::singleShot (0, &download, SLOT (start ()));
		return app.exec ();
	}
	Q_UNREACHABLE ();
	return EXIT_FAILURE;
}
}
