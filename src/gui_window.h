#pragma once
#ifndef GUI_WINDOW_H
#define GUI_WINDOW_H

#include <QApplication>
#include <QCloseEvent>

#include <QItemSelectionModel>
#include <QSystemTrayIcon>

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>

#include <QAction>
#include <QMenu>
#include <QSplitter>

#include "core_localshare.h"
#include "core_server.h"
#include "core_settings.h"
#include "gui_discovery_subsystem.h"
#include "gui_peer_list.h"
#include "gui_style.h"
#include "gui_transfer_list.h"
#include "gui_transfers.h"

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
	Discovery::SubSystem * discovery_subsystem{nullptr};

	QSystemTrayIcon * tray{nullptr};

	QAbstractItemView * peer_list_view{nullptr};
	PeerList::Model * peer_list_model{nullptr};
	TransferList::Model * transfer_list_model{nullptr};

public:
	Window (QWidget * parent = nullptr) : QMainWindow (parent) {
		{
			// Start Server
			auto server = new Transfer::Server (this);
			connect (server, &Transfer::Server::download_ready, this, &Window::new_download);

			// Local peer
			using Discovery::LocalDnsPeer;
			local_peer = new LocalDnsPeer (this);
			local_peer->set_port (server->port ());
			local_peer->set_requested_username (Settings::Username ().get ());
			connect (local_peer, &LocalDnsPeer::requested_username_changed,
			         [=] { Settings::Username ().set (local_peer->get_requested_username ()); });
			connect (local_peer, &LocalDnsPeer::username_changed, this, &Window::set_window_title);

			// Restartable discovery system
			discovery_subsystem = new Discovery::SubSystem (local_peer, this);
			setStatusBar (discovery_subsystem);
			connect (discovery_subsystem, &Discovery::SubSystem::new_discovered_peer, this,
			         &Window::new_discovered_peer);
		}

		// Common actions
		auto action_send_file = new QAction (Icon::send_file (), tr ("Send &File..."), this);
		action_send_file->setShortcuts (QKeySequence::Open); // Only one can get the shortcut...
		action_send_file->setEnabled (false);
		action_send_file->setStatusTip (tr ("Chooses a file to send to selected peers"));
		connect (action_send_file, &QAction::triggered, this, &Window::action_send_file_clicked);

		auto action_send_dir = new QAction (Icon::send_dir (), tr ("Send &Directory..."), this);
		action_send_dir->setEnabled (false);
		action_send_dir->setStatusTip (tr ("Chooses a directory to send to selected peers"));
		connect (action_send_dir, &QAction::triggered, this, &Window::action_send_dir_clicked);

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
				         action_send_file->setEnabled (!selection.isEmpty ());
				         action_send_dir->setEnabled (!selection.isEmpty ());
				       });
		}

		// Transfer table
		{
			auto view = new TransferList::View (splitter);
			auto model = new TransferList::Model (view);
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
			file->addAction (action_send_file);
			file->addAction (action_send_dir);
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

			auto send_hidden_files = new QAction (tr ("Send &hidden files"), pref);
			send_hidden_files->setCheckable (true);
			send_hidden_files->setChecked (Settings::UploadHidden ().get ());
			send_hidden_files->setStatusTip (tr ("Also send hidden files when sending a directory."));
			connect (send_hidden_files, &QAction::triggered,
			         [=](bool checked) { Settings::UploadHidden ().set (checked); });

			auto download_path =
			    new QAction (Icon::change_download_path (), tr ("Set default download &path..."), pref);
			download_path->setStatusTip (tr ("Sets the path used by default to store downloaded files."));
			connect (download_path, &QAction::triggered, [=](void) {
				Settings::DownloadPath path;
				auto new_path =
				    QFileDialog::getExistingDirectory (this, tr ("Set default download path"), path.get ());
				if (!new_path.isEmpty ())
					path.set (new_path);
			});

			auto download_auto =
			    new QAction (Icon::download_auto (), tr ("Always &accept downloads"), pref);
			download_auto->setCheckable (true);
			download_auto->setChecked (Settings::DownloadAuto ().get ());
			download_auto->setStatusTip (tr ("Enable automatic accept of all incoming download offers."));
			connect (download_auto, &QAction::triggered,
			         [=](bool checked) { Settings::DownloadAuto ().set (checked); });

			auto change_username =
			    new QAction (Icon::change_username (), tr ("Change &username..."), pref);
			change_username->setStatusTip ("Set a new username in settings and discovery");
			connect (change_username, &QAction::triggered, [=](void) {
				QString new_username =
				    QInputDialog::getText (this, tr ("Select new username"), tr ("Username:"),
				                           QLineEdit::Normal, local_peer->get_requested_username ());
				if (!new_username.isEmpty ())
					local_peer->set_requested_username (new_username);
			});

			pref->addAction (use_tray);
			pref->addSeparator ();
			pref->addAction (send_hidden_files);
			pref->addAction (download_path);
			pref->addAction (download_auto);
			pref->addSeparator ();
			pref->addAction (change_username);
		}

		// Help menu
		{
			auto help = menuBar ()->addMenu (tr ("&Help"));

			auto about_qt = new QAction (tr ("About &Qt"), help);
			about_qt->setMenuRole (QAction::AboutQtRole);
			about_qt->setStatusTip (tr ("Information about Qt"));
			connect (about_qt, &QAction::triggered, qApp, &QApplication::aboutQt);

			auto about = new QAction (tr ("&About %1").arg (Const::app_display_name), help);
			about->setMenuRole (QAction::AboutRole);
			about->setStatusTip (tr ("Information about %1").arg (Const::app_display_name));
			connect (about, &QAction::triggered, this, &Window::show_about);

			help->addAction (about_qt);
			help->addAction (about);
		}

		// Toolbar
		{
			auto tool_bar = addToolBar (tr ("Application"));
			tool_bar->setMovable (false);
			tool_bar->setObjectName ("toolbar");
			tool_bar->addAction (action_send_file);
			tool_bar->addAction (action_send_dir);
			tool_bar->addAction (action_add_peer);
		}

		setUnifiedTitleAndToolBarOnMac (true);
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
		auto username = local_peer->get_username ();
		if (username.isEmpty ())
			setWindowTitle (tr ("Unregistered"));
		else
			setWindowTitle (username);
	}

	void tray_activated (QSystemTrayIcon::ActivationReason reason) {
		if (reason == QSystemTrayIcon::DoubleClick)
			setVisible (!isVisible ()); // Toggle window visibility
	}

	void action_send_file_clicked (void) {
		// Select and send file to selection if clicked
		auto filepath = QFileDialog::getOpenFileName (this, tr ("Choose file to send"));
		if (filepath.isEmpty ())
			return;
		auto selection = peer_list_view->selectionModel ()->selectedRows ();
		for (auto & index : selection)
			request_upload (peer_list_model->get_item_t<PeerList::Item *> (index)->get_peer (), filepath);
	}

	void action_send_dir_clicked (void) {
		// Select and send directory to selection if clicked
		auto dirpath = QFileDialog::getExistingDirectory (this, tr ("Choose directory to send"));
		if (dirpath.isEmpty ())
			return;
		auto selection = peer_list_view->selectionModel ()->selectedRows ();
		for (auto & index : selection)
			request_upload (peer_list_model->get_item_t<PeerList::Item *> (index)->get_peer (), dirpath);
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
		// TODO move to a more event-like management (for file list building) ?
		auto upload = new Transfer::Upload (peer.username, local_peer->get_username ());
		// Link to item to catch any error, then load files
		auto item = new TransferList::Upload (upload, this);
		if (!upload->set_payload (filepath, Settings::UploadHidden ().get ()))
			return;
		// Only then connect and show the item
		upload->connect (peer.address, peer.port);
		transfer_list_model->append (item);
	}

	void new_download (Transfer::Download * download) {
		auto item = new TransferList::Download (download, this);
		transfer_list_model->append (item);
	}

	// About message

	void show_about (void) {
		auto msg = new QMessageBox (this);
		msg->setAttribute (Qt::WA_DeleteOnClose);
		msg->setIconPixmap (
		    Icon::app ().pixmap (style ()->pixelMetric (QStyle::PM_MessageBoxIconSize)));
		msg->setWindowTitle (tr ("About %1").arg (Const::app_display_name));
		msg->setText (tr ("<p>%1 v%2 is a small file sharing application for the local network.</p>")
		                  .arg (Const::app_display_name)
		                  .arg (Const::app_version));
		msg->setInformativeText (
		    tr ("<p>It is designed to easily send files to peers across the local network. "
		        "It can be viewed as a netcat with auto discovery of peers and a nice interface. "
		        "Drag & drop a file on a peer, "
		        "or select peers and click on send to initiate a transfer. "
		        "It also supports manually adding peers by ip/hostname/port, "
		        "but this will not work if the destination is behind firewalls.</p>"

		        "<p>Be careful of the automatic download option. "
		        "It prevents you from rejecting unwanted file offers, "
		        "and could allow attackers to fill your disk. "
		        "As a general rule, be careful if you use %1 on a public network.</p>"

		        "<p>Without automatic download, you must accept each transfer manually. "
		        "Before accepting, you can change the destination by clicking the directory icon. "
		        "You can also change the default destination in the preferences.</p>"

		        "<p>If using the system tray icon, %1 acts like a small daemon. "
		        "Hiding/closing the window only reduces it to the system tray. "
		        "It can be useful for long transfers, but do not forget to close it !</p>"

		        "<p>%1 also has a command line interface. "
		        "It works without a graphical environment, making it very close to a better netcat. "
		        "Options can be found by calling %1 with option --help.</p>"

		        "<p>Copyright (C) 2016 François Gindraud.</p>"
		        "<p><a href=\"https://github.com/lereldarion/qt-localshare\">Github Link</a></p>")
		        .arg (Const::app_display_name));
		msg->exec ();
	}
};

#endif
