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
#include <QSplitter>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QToolBar>
#include <QTreeView>

class Window : public QMainWindow {
	Q_OBJECT

	/* Main window of application.
	 * If tray icon is supported, closing it will just hide it, and clicking the tray icon toggle its
	 * visibility. Application can be closed by tray menu -> quit.
	 */
private:
	Discovery::LocalDnsPeer * local_peer{nullptr};

	QSystemTrayIcon * tray{nullptr};
	QLabel * status_message{nullptr};

	QAbstractItemView * peer_list_view{nullptr};
	PeerList::Model * peer_list_model{nullptr};
	Transfer::Model * transfer_list_model{nullptr};

public:
	Window (QWidget * parent = nullptr) : QMainWindow (parent) {
		// Start Server
		auto server = new Transfer::Server (this);
		connect (server, &Transfer::Server::new_connection, this, &Window::incoming_connection);

		// Local peer
		local_peer = new Discovery::LocalDnsPeer (server->port (), this);
		connect (local_peer, &Discovery::DnsPeer::name_changed, this, &Window::set_window_title);

		// Discovery setup
		auto service = new Discovery::Service (local_peer, this);
		connect (service, &Discovery::Service::registered, this, &Window::service_registered);

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

		// Main widget = splitter
		auto splitter = new QSplitter (Qt::Vertical, this);
		splitter->setChildrenCollapsible (false);
		setCentralWidget (splitter);

		// Peer table
		{
			auto view = new QTreeView (splitter);
			view->setAlternatingRowColors (true);
			view->setRootIsDecorated (false);
			view->setAcceptDrops (true);
			view->setDropIndicatorShown (true);
			view->setSelectionBehavior (QAbstractItemView::SelectRows);
			view->setSelectionMode (QAbstractItemView::ExtendedSelection);
			view->setSortingEnabled (true);
			view->setMouseTracking (true);
			peer_list_view = view;
			// TODO preset columns width

			auto delegate = new PeerList::Delegate (view);
			view->setItemDelegate (delegate);

			auto model = new PeerList::Model (view);
			view->setModel (model);
			peer_list_model = model;

			connect (view->selectionModel (), &QItemSelectionModel::selectionChanged,
			         [=](const QItemSelection & selection) {
				         action_send->setEnabled (!selection.isEmpty ());
				       });
			connect (delegate, &PeerList::Delegate::button_clicked, model,
			         &PeerList::Model::button_clicked);
		}

		// Transfer table
		{
			auto view = new QTreeView (splitter);
			view->setAlternatingRowColors (true);
			view->setRootIsDecorated (false);
			view->setSelectionBehavior (QAbstractItemView::SelectRows);
			view->setSelectionMode (QAbstractItemView::NoSelection);
			view->setSortingEnabled (true);
			view->setMouseTracking (true);
			// TODO preset columns width

			auto delegate = new Transfer::Delegate (view);
			view->setItemDelegate (delegate);

			auto model = new Transfer::Model (view);
			view->setModel (model);
			transfer_list_model = model;

			connect (delegate, &Transfer::Delegate::button_clicked, model,
			         &Transfer::Model::button_clicked);
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
			connect (about, &QAction::triggered, [=] {
				QMessageBox::about (this, tr ("About Localshare"),
				                    tr ("Localshare v%1 is a small file sharing app for the local network.")
				                        .arg (Const::app_version));
				// TODO improve description
			});

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

		// Status bar
		status_message = new QLabel (tr ("Localshare starting up..."));
		statusBar ()->addWidget (status_message);

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

	void service_registered (QString name) {
		local_peer->set_name (name);

		status_message->setText (tr ("Localshare running on port %1 with username %2")
		                             .arg (local_peer->get_port ())
		                             .arg (local_peer->get_username ()));

		// Start browsing
		auto browser = new Discovery::Browser (local_peer, this);
		connect (browser, &Discovery::Browser::added, this, &Window::new_discovered_peer);
	}

	void tray_activated (QSystemTrayIcon::ActivationReason reason) {
		switch (reason) {
		case QSystemTrayIcon::DoubleClick: // Only double click
			setVisible (!isVisible ());      // Toggle window visibility
			break;
		default:
			break;
		}
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

	void request_upload (const Peer & peer, const QString & filepath) {
		auto upload = new Transfer::Upload (peer, filepath, local_peer->get_username (), transfer_list_model);
		transfer_list_model->append (upload);
	}

	void incoming_connection (QAbstractSocket * connection) {
		auto download = new Transfer::Download (connection);
		transfer_list_model->append (download);
	}
};

#endif
