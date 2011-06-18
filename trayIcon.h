#ifndef H_TRAYICON
#define H_TRAYICON

#include "localshare.h"

#include <QtGui>

class TrayIcon : public QSystemTrayIcon {
	Q_OBJECT

	public:
		TrayIcon ();
		~TrayIcon ();

	private slots:
		void about (void);

	private:
		void createMenu (void);

		QMenu * contextMenu;

		QAction * aboutAction;
		QAction * exitAction;
};

#endif

