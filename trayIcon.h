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

		QMenu * m_context;

		QAction * m_about;
		QAction * m_exit;
};

#endif

