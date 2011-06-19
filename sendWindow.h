#ifndef H_SENDWINDOW
#define H_SENDWINDOW

#include "localshare.h"
#include "common.h"

#include <QUdpSocket>
#include <QByteArray>
#include <QtGui>

class SendWindow : public QWidget {
	Q_OBJECT

	public:
		SendWindow ();

	public slots:
		void toggled (void);

	private:
		QLabel * m_label;
};

#endif

