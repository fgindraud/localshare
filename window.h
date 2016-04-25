#ifndef WINDOW_H
#define WINDOW_H

#include "localshare.h"
#include "style.h"

#include <QtWidgets>

class Window : public QWidget {
	Q_OBJECT

	/* Main window of application.
	 * If tray icon is supported, closing it will just hide it, and clicking the tray icon toggle its
	 * visibility. Application can be closed by tray menu -> quit.
	 */
private:
	QSystemTrayIcon * tray{nullptr};

public:
	Window () {
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
};

#endif
