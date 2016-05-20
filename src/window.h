#ifndef WINDOW_H
#define WINDOW_H

#include "discovery.h"
#include "localshare.h"
#include "peer_list.h"
#include "settings.h"
#include "style.h"
#include "transfer_download.h"
#include "transfer_list.h"
#include "transfer_server.h"
#include "transfer_upload.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QToolBar>

/* RestartableDiscovery system.
 * It allows to dynamically start the discovery subsystem.
 *
 * Browser and Service are (almost) independent.
 * Browser depends on the service instance name.
 * This name is preset in local_peer, and may change after registration.
 * Thankfully Browser does not need to restart, as the name is only used for filtering.
 *
 * This widget acts as a Status Bar.
 * It normally displays the server port and the registration status.
 * In case of errors, it grows with a big warning icon and a restart button.
 * This button will clear errors and restart all dead services.
 * Service and Browser will emit being_destroyed on destruction, with a possible error.
 */
class RestartableDiscovery : public QStatusBar {
	Q_OBJECT

private:
	Discovery::LocalDnsPeer * local_peer;

	QPointer<Discovery::Service> service;
	QPointer<Discovery::Browser> browser;

	QString current_errors{"Init"};
	QLabel * warning_symbol{nullptr};
	QLabel * message{nullptr};
	QPushButton * restart{nullptr};

signals:
	void new_discovered_peer (Discovery::DnsPeer * peer);

public:
	RestartableDiscovery (Discovery::LocalDnsPeer * local_peer, QWidget * parent = nullptr)
	    : QStatusBar (parent), local_peer (local_peer) {
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
		connect (restart, &QPushButton::clicked, this, &RestartableDiscovery::start_services);

		// Start services for the first time
		start_services ();
	}

private slots:
	void start_services (void) {
		// Restart all dead services
		if (!service) {
			service = new Discovery::Service (local_peer);
			connect (service.data (), &Discovery::Service::registered, this,
			         &RestartableDiscovery::message_changed);
			connect (service.data (), &Discovery::Service::being_destroyed, this,
			         &RestartableDiscovery::append_error);
		}
		if (!browser) {
			browser = new Discovery::Browser (local_peer);
			connect (browser.data (), &Discovery::Browser::added, this,
			         &RestartableDiscovery::new_discovered_peer);
			connect (browser.data (), &Discovery::Browser::being_destroyed, this,
			         &RestartableDiscovery::append_error);
		}
		clear_errors ();
	}

	void message_changed (void) { message->setText (make_status_message () + current_errors); }

	void append_error (const QString & error) {
		if (error.isEmpty ())
			return; // ignore
		if (current_errors.isEmpty ()) {
			insertWidget (0, warning_symbol);
			warning_symbol->show ();
			addWidget (restart);
			restart->show ();
		}
		current_errors += "\n" + error;
		message_changed ();
	}

	void clear_errors (void) {
		if (!current_errors.isEmpty ()) {
			removeWidget (warning_symbol);
			removeWidget (restart);
			current_errors.clear ();
			message_changed ();
		}
	}

private:
	QString make_status_message (void) const {
		if (service) {
			if (service->is_registered ()) {
				return tr ("Localshare running on port %1 and registered with username \"%2\".")
				    .arg (local_peer->get_port ())
				    .arg (local_peer->get_username ());
			} else {
				return tr ("Localshare running on port %1 and registering...")
				    .arg (local_peer->get_port ());
			}
		} else {
			return tr ("Localshare running on port %1 and unregistered (error) !")
			    .arg (local_peer->get_port ());
		}
	}
};

/* Main window of application.
 * Handles most high level GUI functions (the rest is provided by view/models).
 * It also link together functionnality from peer list, transfer list, discovery.
 *
 * If tray icon is supported, closing it will just hide it, and clicking the tray icon toggle its
 * visibility. Application can be closed by tray menu -> quit.
 *
 * The Transfer Server should be alive for the lifetime of Window.
 */
class Window : public QMainWindow {
	Q_OBJECT

private:
	Discovery::LocalDnsPeer * local_peer{nullptr};

	QSystemTrayIcon * tray{nullptr};

	RestartableDiscovery * restartable_discovery{nullptr};

	QAbstractItemView * peer_list_view{nullptr};
	PeerList::Model * peer_list_model{nullptr};
	Transfer::Model * transfer_list_model{nullptr};

public:
	Window (QWidget * parent = nullptr) : QMainWindow (parent) {
		{
			// Start Server
			auto server = new Transfer::Server (this);
			connect (server, &Transfer::Server::new_connection, this, &Window::incoming_connection);

			// Local peer
			local_peer = new Discovery::LocalDnsPeer (server->port (), this);
			connect (local_peer, &Discovery::LocalDnsPeer::name_changed, this, &Window::set_window_title);

			// Restartable discovery system
			restartable_discovery = new RestartableDiscovery (local_peer, this);
			setStatusBar (restartable_discovery);

			connect (restartable_discovery, &RestartableDiscovery::new_discovered_peer, this,
			         &Window::new_discovered_peer);
		}

		// Common actions
		auto action_send = new QAction (Icon::send (), tr ("&Send..."), this);
		action_send->setShortcuts (QKeySequence::Open);
		action_send->setEnabled (false);
		action_send->setStatusTip (tr ("Chooses a file to send to selected peers"));
		connect (action_send, &QAction::triggered, this, &Window::action_send_clicked);

		auto action_add_peer = new QAction (Icon::add_peer (), tr ("&Add manual peer"), this);
		action_add_peer->setStatusTip (tr ("Add a peer entry to fill manually"));
		connect (action_add_peer, &QAction::triggered, this, &Window::new_manual_peer);

		auto action_quit = new QAction (Icon::quit (), tr ("&Quit"), this);
		action_quit->setShortcuts (QKeySequence::Quit);
		action_quit->setMenuRole (QAction::QuitRole);
		action_quit->setStatusTip (tr ("Exits the application"));
		connect (action_quit, &QAction::triggered, qApp, &QCoreApplication::quit);

		// Main widget is a splitter
		auto splitter = new QSplitter (Qt::Vertical, this);
		splitter->setChildrenCollapsible (false);
		setCentralWidget (splitter);

		// Peer table
		{
			auto view = new PeerList::View (splitter);
			peer_list_view = view;

			auto model = new PeerList::Model (view);
			view->setModel (model);
			peer_list_model = model;

			connect (view->selectionModel (), &QItemSelectionModel::selectionChanged,
			         [=](const QItemSelection & selection) {
				         action_send->setEnabled (!selection.isEmpty ());
				       });
		}

		// Transfer table
		{
			auto view = new Transfer::View (splitter);
			auto model = new Transfer::Model (view);
			view->setModel (model);
			transfer_list_model = model;
		}

		// System tray
		auto setting_show_tray = Settings::UseTray ().get ();
		tray = new QSystemTrayIcon (this);
		tray->setIcon (Icon::app ());
		tray->setVisible (setting_show_tray);
		connect (tray, &QSystemTrayIcon::activated, this, &Window::tray_activated);

		{
			auto menu = new QMenu (this); // cannot be child of tray (not Widget)
			tray->setContextMenu (menu);

			auto show_window = new QAction (Icon::restore (), tr ("Show &Window"), menu);
			connect (show_window, &QAction::triggered, this, &QWidget::show);

			menu->addAction (show_window);
			menu->addSeparator ();
			menu->addAction (action_quit);
		}

		// File menu
		{
			auto file = menuBar ()->addMenu (tr ("&Application"));
			file->addAction (action_send);
			file->addAction (action_add_peer);
			file->addSeparator ();
			file->addAction (action_quit);
		}

		// Preferences menu
		{
			auto pref = menuBar ()->addMenu (tr ("&Preferences"));

			auto use_tray = new QAction (tr ("Use System &Tray"), pref);
			use_tray->setCheckable (true);
			use_tray->setChecked (setting_show_tray);
			use_tray->setStatusTip (tr ("Enables use of persistent system tray icon"));
			connect (use_tray, &QAction::triggered, tray, &QSystemTrayIcon::setVisible);
			connect (use_tray, &QAction::triggered,
			         [=](bool checked) { Settings::UseTray ().set (checked); });

			auto download_path = new QAction (tr ("Set default download &path..."), pref);
			download_path->setStatusTip (tr ("Sets the path used by default to store downloaded files."));
			connect (download_path, &QAction::triggered, [=](void) {
				Settings::DownloadPath path;
				auto new_path =
				    QFileDialog::getExistingDirectory (this, tr ("Set default download path"), path.get ());
				if (!new_path.isEmpty ())
					path.set (new_path);
			});

			auto download_auto = new QAction (tr ("Always &accept downloads"), pref);
			download_auto->setCheckable (true);
			download_auto->setChecked (Settings::DownloadAuto ().get ());
			download_auto->setStatusTip (tr ("Enable automatic accept of all incoming download offers."));
			connect (download_auto, &QAction::triggered,
			         [=](bool checked) { Settings::DownloadAuto ().set (checked); });

			// TODO change username

			pref->addAction (use_tray);
			pref->addSeparator ();
			pref->addAction (download_path);
			pref->addAction (download_auto);
		}

		// Help menu
		{
			auto help = menuBar ()->addMenu (tr ("&Help"));

			auto about_qt = new QAction (tr ("About &Qt"), help);
			about_qt->setMenuRole (QAction::AboutQtRole);
			about_qt->setStatusTip (tr ("Information about Qt"));
			connect (about_qt, &QAction::triggered, qApp, &QApplication::aboutQt);

			auto about = new QAction (tr ("&About Localshare"), help);
			about->setMenuRole (QAction::AboutRole);
			about->setStatusTip (tr ("Information about Localshare"));
			connect (about, &QAction::triggered, this, &Window::show_about);

			help->addAction (about_qt);
			help->addAction (about);
		}

		// Toolbar
		{
			auto tool_bar = addToolBar (tr ("Application"));
			tool_bar->setObjectName ("toolbar");
			tool_bar->addAction (action_send);
			tool_bar->addAction (action_add_peer);
		}

		set_window_title ();
		restoreGeometry (Settings::Geometry ().get ());
		restoreState (Settings::WindowState ().get ());
		show (); // Show everything
	}

	~Window () {
		Settings::Geometry ().set (saveGeometry ());
		Settings::WindowState ().set (saveState ());
	}

protected:
	void closeEvent (QCloseEvent * event) {
		if (event->spontaneous () && isVisible () && tray->isVisible ()) {
			// If tray is used, and user asked to close the window, then hide it instead.
			hide ();
			event->ignore ();
			// It accepts the close if the window was not visible (ex: OSX close request from menu)
		} else {
			event->accept ();
		}
	}

private slots:
	void set_window_title (void) {
		setWindowTitle (tr ("Localshare - %1").arg (local_peer->get_username ()));
	}

	void tray_activated (QSystemTrayIcon::ActivationReason reason) {
		if (reason == QSystemTrayIcon::DoubleClick)
			setVisible (!isVisible ()); // Toggle window visibility
	}

	void action_send_clicked (void) {
		// Select and send file to selection if clicked
		auto filepath = QFileDialog::getOpenFileName (this, tr ("Choose file to send..."));
		if (filepath.isEmpty ())
			return;
		auto selection = peer_list_view->selectionModel ()->selectedIndexes ();
		for (auto & index : selection) {
			if (index.column () == 0 && peer_list_model->has_item (index)) {
				// TreeView selected row generates 4 selection items ; only keep 1 per row
				request_upload (peer_list_model->get_item_t<PeerList::Item *> (index)->get_peer (),
				                filepath);
			}
		}
	}

	// Peer creation

	void new_manual_peer (void) {
		auto item = new PeerList::ManualItem (peer_list_model);
		connect (item, &PeerList::Item::request_upload, this, &Window::request_upload);
		peer_list_model->append (item);
	}

	void new_discovered_peer (Discovery::DnsPeer * peer) {
		auto item = new PeerList::DiscoveryItem (peer);
		connect (item, &PeerList::Item::request_upload, this, &Window::request_upload);
		peer_list_model->append (item);
	}

	// Transfer creation

	void request_upload (const Peer & peer, const QString & filepath) {
		auto upload =
		    new Transfer::Upload (peer, filepath, local_peer->get_username (), transfer_list_model);
		transfer_list_model->append (upload);
	}

	void incoming_connection (QAbstractSocket * connection) {
		auto download = new Transfer::Download (connection);
		transfer_list_model->append (download);
	}

	// About message

	void show_about (void) {
		QMessageBox::about (
		    this, tr ("About Localshare"),
		    tr ("<p>Localshare v%1 is a small file sharing app for the local network.</p>"

		        "<p>It is designed to easily send files to peers across the local network. "
		        "It can be viewed as a netcat with auto discovery of peers and a nice interface. "
		        "Drag & drop a file on a peer, "
		        "or select peers and click on send to initiate a transfer. "
		        "It also supports manually adding peers by ip/hostname/port, "
		        "but this will not work if the destination is behind firewalls.</p>"

		        "<p>Be careful of the automatic download option. "
		        "It prevents you from rejecting unwanted file offers, "
		        "and could allow attackers to fill your disk. "
		        "As a general rule, be careful if you use Localshare on a public network.</p>"

		        "<p>Without automatic download, you must accept each transfer manually. "
		        "Before accepting, you can change the destination by clicking the directory icon. "
		        "You can also change the default destination in the preferences.</p>"

		        "<p>If using the system tray icon, Localshare acts like a small daemon. "
		        "Hiding/closing the window only reduces it to the system tray. "
		        "It can be useful for long transfers, but do not forget to close it !</p>"

		        "<p>Copyright (C) 2016 François Gindraud.</p>"
		        "<p><a href='https://github.com/lereldarion/qt-localshare'>Github Link</a></p>")
		        .arg (Const::app_version));
	}
};

#endif
