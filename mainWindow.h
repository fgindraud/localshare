#ifndef H_MAINWINDOW
#define H_MAINWINDOW

#include "localshare.h"
#include "common.h"

#include <QtGui>

class MainWindow : public QWidget {
	Q_OBJECT

	public:
		MainWindow ();

	public slots:
		void toggled (void);

	private:
		QLabel * m_label;
};

#endif

