#ifndef H_SENDWINDOW
#define H_SENDWINDOW

#include "localshare.h"

#include <QtGui>

class SendWindow : public QWidget {
	Q_OBJECT

	public:
		SendWindow ();

	private:

		QVBoxLayout * mainLayout;


};

#endif

