/*
 * Network abstraction
 */
#ifndef H_NETWORK
#define H_NETWORK

#include "localshare.h"
#include "common.h"
#include "qtzeroconf/zconfservicebrowser.h"
#include "qtzeroconf/zconfservice.h"

#include <QtCore>
#include <QtNetwork>

/*
 * Handles all zeroconf discovery system interactions
 */

struct ZeroconfPeer {
	QHostAddress m_address;
	QString m_name;
	quint16 m_port;

	ZeroconfPeer () {}
	ZeroconfPeer (ZConfServiceEntry & entry) :
		m_address (entry.ip),
		m_name (entry.host),
		m_port (entry.port)
	{}
};

class ZeroconfHandler : public QObject {
	Q_OBJECT
	
	public:
		ZeroconfHandler (Settings & settings);
		void start (void);

	signals:
		void addPeer (ZeroconfPeer);
		void removePeer (QString);

	public slots:
		// Not implemented yet
		void settingsChanged (void);

	private slots:
		void internalAddPeer (QString name);
		void internalRemovePeer (QString name);

	private:
		ZConfServiceBrowser m_browser;
		ZConfService m_service;

		// If any of this settings are changed, we must
		QString m_name;
		quint16 m_port;
};

#endif

