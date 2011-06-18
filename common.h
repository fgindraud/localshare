#ifndef H_COMMON
#define H_COMMON

#include <QtCore>

class Settings : private QSettings {
	public:
		Settings ();

		/* network */
		QString name (void) const;
		void setName (QString & name);

		quint16 udpPort (void) const;
		void setUdpPort (quint16 port);

		quint16 tcpPort (void) const;
		void setTcpPort (quint16 port);

		/* download */
		QString downloadPath (void) const;
		void setDownloadPath (QString & path);

		bool alwaysDownload (void) const;
		void setAlwaysDownload (bool always);
};

#endif

