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

		peer_list << Peer{"bloh", "kitty", QHostAddress ("192.44.29.1"), 42};
		peer_list << Peer{"John", "eurobeat", QHostAddress ("8.8.8.8"), 1000};
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

class Window : public QWidget {
	Q_OBJECT

	/* Main window of application.
	 * If tray icon is supported, closing it will just hide it, and clicking the tray icon toggle its
	 * visibility. Application can be closed by tray menu -> quit.
	 */
private:
	QSystemTrayIcon * tray{nullptr};
	QPushButton * send_file_button{nullptr};
	QAbstractItemView * peer_list_view{nullptr};

public:
	Window (const QString & blah) {
		// Start server
		auto server = new Transfer::Server (this);

		// Discovery setup
		auto username = Settings::Username ().get () + blah;
		auto service = new Discovery::Service (username, Const::service_name, server->port (), this);
		connect (service, &Discovery::Service::registered, this, &Window::service_registered);

		// Layout
		auto layout = new QGridLayout ();
		setLayout (layout);

		// Peer table
		send_file_button = new QPushButton (tr ("&Send file..."), this);
		send_file_button->setEnabled (false);
		layout->addWidget (send_file_button, 0, 1, Qt::AlignCenter);

		peer_list_view = new QListView (this);
		peer_list_view->setAlternatingRowColors (true);
		peer_list_view->setAcceptDrops (true);
		peer_list_view->setDropIndicatorShown (true);
		peer_list_view->setSelectionMode (QAbstractItemView::ExtendedSelection);
		layout->addWidget (peer_list_view, 0, 0);

		// System tray
		tray = new QSystemTrayIcon (this);
		tray->setIcon (Icon::app ());
		tray->show ();
		connect (tray, &QSystemTrayIcon::activated, this, &Window::tray_activated);

		{
			// Tray menu
			auto menu = new QMenu (this);
			auto action_quit = new QAction (tr ("&Quit"), menu);
			connect (action_quit, &QAction::triggered, qApp, &QCoreApplication::quit);

			menu->addAction (action_quit);
			tray->setContextMenu (menu);
		}

		// Window
		setWindowFlags (Qt::Window);
		setWindowTitle (Const::app_name);
		show ();
	}

protected:
	void closeEvent (QCloseEvent * event) {
		if (tray->isVisible ()) {
			// Do not kill app, just hide window
			hide ();
			event->ignore ();
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

		// Send_file button : enable if something is selected
		connect (peer_list_view->selectionModel (), &QItemSelectionModel::selectionChanged,
		         [=](const QItemSelection & selection) {
			         send_file_button->setEnabled (!selection.isEmpty ());
			       });
		// Select and send file to selection if clicked
		connect (send_file_button, &QPushButton::clicked, [=] {
			QString path = QFileDialog::getOpenFileName (this, tr ("Send file..."));
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
		case QSystemTrayIcon::Trigger:
		case QSystemTrayIcon::DoubleClick:
			setVisible (!isVisible ()); // Toggle window visibility
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
