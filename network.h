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

/* ------ Zeroconf wrapper ------ */

/*
 * Store zeroconf data in a Qt-ish struct
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

/*
 * Wrapper for zeroconf browsing and service announcer
 * It will retrieve all info from settings directly
 */
class ZeroconfHandler : public QObject {
	Q_OBJECT
	
	public:
		ZeroconfHandler ();

		// Delayed start, to let the tcp server start before announcing it
		void start (void);

		/*
		 * Here, we create signals directly giving info (instead of giving only
		 * the name)
		 */
	signals:
		void addPeer (ZeroconfPeer &);
		void removePeer (QString &);

	private slots:
		void internalAddPeer (QString name);
		void internalRemovePeer (QString name);

	private:
		ZConfServiceBrowser mBrowser;
		ZConfService mService;
};

/*
 * Tcp layer
 *
 * Protocol sender->receiver :
 * connection closed = abort
 *
 * --[quint64 peerNameSize]-->
 * --[char[] peerName]-->
 * --[quint64 fileNameSize]-->
 * --[char[] fileName]-->
 * --[quint64 fileSize]-->
 * <--[quint64 accepted]--
 * --[char[] fileContent]-->
 *
 */

class InTransferHandler;
class OutTransferHandler;

class TcpServer : private QTcpServer {
	Q_OBJECT
	
	public:
		TcpServer ();
		~TcpServer ();

	signals:
		void newConnection (InTransferHandler * handler);

	private slots:
		void handleNewConnectionInternal (void);

	private:
		QList<InTransferHandler *> mWaitingForHeaderConnections;
};

class InTransferHandler : private QObject {
	Q_OBJECT

	public:
		InTransferHandler (QTcpSocket * socket);
		~InTransferHandler ();

	private slots:
		void processData (void);
	
	private:
		QTcpSocket * mSocket;

		// State
		bool mReceivedPeerName;
		bool mReceivedFileName;
		bool mReceivedFileSize;
};

class OutTransferHandler : private QObject {
	Q_OBJECT
	

};

#endif

