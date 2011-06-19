#ifndef H_COMMON
#define H_COMMON

#include <QtCore>
#include <QHostAddress>

class Settings : private QSettings {
	public:
		Settings ();

		/* network */
		QString name (void) const;
		void setName (const QString & name);

		quint16 udpPort (void) const;
		void setUdpPort (quint16 port);

		quint16 tcpPort (void) const;
		void setTcpPort (quint16 port);

		/* download */
		QString downloadPath (void) const;
		void setDownloadPath (const QString & path);

		bool alwaysDownload (void) const;
		void setAlwaysDownload (bool always);
};

class Peer {
	public:
		Peer (const QString & name, const QHostAddress & address);
		QString name (void) const;
		const QHostAddress & address (void) const;

		typedef QList<Peer> List;

	private:
		QString m_name;
		QHostAddress m_address;
};

bool operator== (const Peer & a, const Peer & b);

class Message {
	public:
		static void error (const QString & title, const QString & message);
		static void warning (const QString & title, const QString & message);
};

#endif

