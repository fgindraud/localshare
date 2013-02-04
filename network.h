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
		void addPeer (ZeroconfPeer &);
		void removePeer (QString &);

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

class TransferHandler : private QObject {
	Q_OBJECT

	public:
		enum Step {
			Init, Waiting, Transferring, Finished
		};

		TransferHandler (QTcpSocket * socket);
		~TransferHandler ();
	
	private:
		Step mStep;
		QTcpSocket * mSocket;
};

class TcpServer : private QTcpServer {
	Q_OBJECT
	
	public:
		TcpServer (Settings & settings);
		~TcpServer ();

	signals:
		void newConnection (TransferHandler * handler);

	public slots:
		void handlerInitComplete (TransferHandler * handler);

	private slots:
		void handleNewConnectionInternal (void);

	private:
		QList<TransferHandler *> mWaitingForHeaderConnections;
};

#endif

