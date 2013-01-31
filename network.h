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
	QHostAddress address;
	QString name;
	QString hostname;
	quint16 port;

	ZeroconfPeer () {}
	ZeroconfPeer (QString _name, ZConfServiceEntry & entry) :
		address (entry.ip),
		name (_name),
		hostname (entry.host),
		port (entry.port)
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
		ZConfServiceBrowser mBrowser;
		ZConfService mService;

		// Cached settings. if settings changed, must reboot avahiService
		QString mName;
		quint16 mPort;
};

/*
 * Tcp layer
 */

class TransferHandler : private QTcpSocket {
	Q_OBJECT

	public:
		enum Step {
			Waiting, Transferring, Finished
		};

		TransferHandler ();
	
	private:
		Step mStep;
};

class TcpServer : private QTcpServer {
	Q_OBJECT
	
	public:
		TcpServer (Settings & settings);

	signals:
		void newConnection (TransferHandler * handler);
};

#endif

