#ifndef H_UDPDISCOVERY
#define H_UDPDISCOVERY

#include "localshare.h"
#include "common.h"

#include <QUdpSocket>
#include <QByteArray>

class UdpDiscovery : public QObject {
	Q_OBJECT

	public:
		UdpDiscovery ();
		
	public slots:
		void newPing (void);

	signals:
		void updateList (const Peer::List & list);

	private slots:
		void datagramsReceived (void);

	private:
		QUdpSocket * m_socket;
		quint8 m_sequenceNumber;
		Peer::List m_peerList;
		quint16 m_port;

		void handleDatagram (const QHostAddress & address, const QByteArray & dg);
};

#endif

