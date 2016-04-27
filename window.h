#ifndef WINDOW_H
#define WINDOW_H

#include "localshare.h"
#include "discovery.h"
#include "settings.h"
#include "transfer.h"
#include "style.h"

#include <QtWidgets>

class PeerListModel : public QAbstractListModel {
	Q_OBJECT

	/* List of peers (using Qt model-view).
	 * Is automatically updated by discovery browser.
	 */
private:
	QList<Peer> peer_list;

signals:
	void request_upload (Peer, QString);

public:
	PeerListModel (const QString & our_username, QObject * parent = nullptr)
	    : QAbstractListModel (parent) {
		auto browser = new Discovery::Browser (our_username, Const::service_name, this);
		connect (browser, &Discovery::Browser::added, this, &PeerListModel::peer_added);
		connect (browser, &Discovery::Browser::removed, this, &PeerListModel::peer_removed);

		// FIXME remove
		peer_list << Peer{"test_bloh", "kitty", QHostAddress ("192.44.29.1"), 42};
		peer_list << Peer{"test_john", "eurobeat", QHostAddress ("8.8.8.8"), 1000};
	}

	// Peer access
	bool has_peer (const QModelIndex & index) const {
		return index.isValid () && index.column () == 0 && 0 <= index.row () &&
		       index.row () < peer_list.size ();
	}
	Peer & get_peer (const QModelIndex & index) { return peer_list[index.row ()]; }
	const Peer & get_peer (const QModelIndex & index) const { return peer_list.at (index.row ()); }

	// View interface
	int rowCount (const QModelIndex & = QModelIndex ()) const { return peer_list.size (); }

	QVariant data (const QModelIndex & index, int role) const {
		if (has_peer (index) && role == Qt::DisplayRole) {
			auto & peer = get_peer (index);
			return QString ("%1 @ %2 [%3 ; %4]")
			    .arg (peer.username)
			    .arg (peer.hostname)
			    .arg (peer.address.toString ())
			    .arg (peer.port);
		} else {
			return QVariant ();
		}
	}

	QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const {
		if (section == 0 && orientation == Qt::Horizontal && role == Qt::DisplayRole) {
			return QString ("Peers");
		} else {
			return QVariant ();
		}
	}

	// Support drop of URIs
	Qt::ItemFlags flags (const QModelIndex & index) const {
		if (index.isValid ()) {
			return QAbstractListModel::flags (index) | Qt::ItemIsDropEnabled;
		} else {
			return 0;
		}
	}

	bool canDropMimeData (const QMimeData * mimedata, Qt::DropAction, int, int,
	                      const QModelIndex &) const {
		// do not check drop position : it seems to block updates of status
		return mimedata->hasUrls ();
	}

	bool dropMimeData (const QMimeData * mimedata, Qt::DropAction action, int row, int column,
	                   const QModelIndex & parent) {
		if (!canDropMimeData (mimedata, action, row, column, parent))
			return false;
		if (action == Qt::IgnoreAction)
			return true; // As in the tutorial... dunno if useful
		// Drop on item <=> row=col=-1, parent.row/col indicates position
		if (!(row == -1 && column == -1 && parent.isValid ()))
			return false;
		if (!has_peer (parent))
			return false;
		auto & peer = get_peer (parent);
		for (auto & url : mimedata->urls ())
			if (url.isValid () && url.isLocalFile ())
				emit request_upload (peer, url.toLocalFile ());
		return true;
	}

private slots:
	void peer_added (const Peer & peer) {
		// Add to end
		beginInsertRows (QModelIndex (), peer_list.size (), peer_list.size ());
		peer_list << peer;
		endInsertRows ();
	}
	void peer_removed (const QString & username) {
		// Find
		for (auto i = 0; i < peer_list.size (); ++i) {
			if (peer_list.at (i).username == username) {
				beginRemoveRows (QModelIndex (), i, i);
				peer_list.removeAt (i);
				endRemoveRows ();
				return;
			}
		}
		qCritical () << "PeerListModel: peer not found:" << username;
	}
};

class Window : public QMainWindow {
	Q_OBJECT

	/* Main window of application.
	 * If tray icon is supported, closing it will just hide it, and clicking the tray icon toggle its
	 * visibility. Application can be closed by tray menu -> quit.
	 */
private:
	QSystemTrayIcon * tray{nullptr};
	QAction * action_send{nullptr};
	QAbstractItemView * peer_list_view{nullptr};

public:
	Window (const QString & blah, QWidget * parent = nullptr) : QMainWindow (parent) {
		// Start server
		auto server = new Transfer::Server (this);

		// Discovery setup
		auto username = Settings::Username ().get () + blah;
		auto service = new Discovery::Service (username, Const::service_name, server->port (), this);
		connect (service, &Discovery::Service::registered, this, &Window::service_registered);

		// Common actions
		action_send = new QAction (Icon::send (), tr ("&Send..."), this);
		action_send->setShortcuts (QKeySequence::Open);
		action_send->setToolTip ("Selects a file to send to selected peers (requires selection)");
		action_send->setEnabled (false);

		auto action_quit = new QAction (Icon::quit (), tr ("&Quit"), this);
		action_quit->setShortcuts (QKeySequence::Quit);
		action_quit->setMenuRole (QAction::QuitRole);
		connect (action_quit, &QAction::triggered, qApp, &QCoreApplication::quit);

		// Peer table
		peer_list_view = new QListView (this);
		peer_list_view->setAlternatingRowColors (true);
		peer_list_view->setAcceptDrops (true);
		peer_list_view->setDropIndicatorShown (true);
		peer_list_view->setSelectionMode (QAbstractItemView::ExtendedSelection);
		setCentralWidget (peer_list_view);

		// System tray
		auto setting_show_tray = Settings::UseTray ().get ();
		tray = new QSystemTrayIcon (this);
		tray->setIcon (Icon::app ());
		tray->setVisible (setting_show_tray);
		connect (tray, &QSystemTrayIcon::activated, this, &Window::tray_activated);

		{
			auto show_window = new QAction (tr ("Show &Window"), this);
			connect (show_window, &QAction::triggered, this, &QWidget::show);

			auto menu = new QMenu (this);
			menu->addAction (show_window);
			menu->addSeparator ();
			menu->addAction (action_quit);
			tray->setContextMenu (menu);
		}

		// Window
		setWindowTitle (Const::app_name);
		{
			auto file = menuBar ()->addMenu (tr ("&File"));
			file->addAction (action_send);
			file->addSeparator ();
			file->addAction (action_quit);

			// Pref
			auto use_tray = new QAction (tr ("Use System &Tray"), this);
			use_tray->setCheckable (true);
			use_tray->setChecked (setting_show_tray);
			connect (use_tray, &QAction::triggered, [=](bool checked) {
				Settings::UseTray ().set (checked);
				tray->setVisible (checked);
			});

			auto pref = menuBar ()->addMenu (tr ("&Preferences"));
			pref->addAction (use_tray);

			// About
			auto about_qt = new QAction (tr ("About &Qt"), this);
			about_qt->setMenuRole (QAction::AboutQtRole);
			connect (about_qt, &QAction::triggered, qApp, &QApplication::aboutQt);

			auto about = new QAction (tr ("&About"), this);
			about->setMenuRole (QAction::AboutRole);
			connect (about, &QAction::triggered, [=] {
				QMessageBox::about (this, tr ("About Localshare"),
				                    tr ("Localshare is a small file sharing app for the local network."));
				// TODO improve description
			});

			auto help = menuBar ()->addMenu (tr ("&Help"));
			help->addAction (about_qt);
			help->addAction (about);
		}
		show ();
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
	void service_registered (QString username) {
		// Complete the window when final username has been received from service registration
		setWindowTitle (QString ("%1 - %2").arg (Const::app_name).arg (username));

		// Start browsing and link to view
		auto peer_list_model = new PeerListModel (username, peer_list_view);
		peer_list_view->setModel (peer_list_model);
		connect (peer_list_model, &PeerListModel::request_upload, this, &Window::upload_requested);

		// Send action : enable if something is selected
		connect (
		    peer_list_view->selectionModel (), &QItemSelectionModel::selectionChanged,
		    [=](const QItemSelection & selection) { action_send->setEnabled (!selection.isEmpty ()); });
		// Select and send file to selection if clicked
		connect (action_send, &QAction::triggered, [=] {
			QString path = QFileDialog::getOpenFileName (this, tr ("Choose file..."));
			if (path.isEmpty ())
				return;
			auto selection = peer_list_view->selectionModel ()->selectedIndexes ();
			for (auto & index : selection)
				if (peer_list_model->has_peer (index))
					emit upload_requested (peer_list_model->get_peer (index), path);
		});
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

	void upload_requested (const Peer & peer, const QString & path) {
		qDebug () << "upload_requested" << peer << path;
	}
};

#endif
