/*
 * TrayIcon
 * Can invoke mainwindow on click.
 * Houses links to utilities (about/conf)
 */
#ifndef H_TRAYICON
#define H_TRAYICON

#include <QtGui>

class TrayIcon : public QSystemTrayIcon {
	Q_OBJECT

	public:
		TrayIcon ();
		~TrayIcon ();

	signals:
		void mainWindowToggled (void);

	private slots:
		void about (void);
		void wasClicked (QSystemTrayIcon::ActivationReason reason);

	private:
		void createMenu (void);

		QMenu * mContext;

		QAction * mAbout;
		QAction * mExit;
};

#endif

