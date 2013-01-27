/*
 * Main window.
 * Heavily uses drag & drop
 */
#ifndef H_MAINWINDOW
#define H_MAINWINDOW

#include "network.h"
#include "common.h"

#include <QtGui>

class MainWindow : public QWidget {
	Q_OBJECT

	public:
		MainWindow (Settings & seetings, ZeroconfHandler & discoveryHandler);
		~MainWindow ();

		void closeEvent (QCloseEvent * event);

	public slots:
		void toggled (void);

	private:
		void createMainWindow (void);

		QLabel * m_label;
};

#endif

