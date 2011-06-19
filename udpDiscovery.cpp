#include "udpDiscovery.h"

UdpDiscovery::UdpDiscovery () : QObject (), m_sequenceNumber (0) {
	Settings settings;
	m_port = settings.udpPort ();

	m_socket = new QUdpSocket (this);

	if (not m_socket->bind (m_port)) {
		QString msg = tr ("Unable to bind port %1."). arg (m_port);
		Message::error (tr ("Network error"), msg);
	}

	QObject::connect (m_socket, SIGNAL (readyRead ()), this, SLOT (datagramsReceived ()));
}

void UdpDiscovery::newPing (void) {
	// clean previous discovery
	++m_sequenceNumber;
	m_peerList.clear ();
	emit updateList (m_peerList);

	// send new ping
	QByteArray packet;
	packet.push_back ((quint8) UDP_TYPE_PING);
	packet.push_back ((quint8) m_sequenceNumber);

	m_socket->writeDatagram (packet, QHostAddress::Broadcast, m_port);
}

void UdpDiscovery::datagramsReceived (void) {
	// handle all pending datagrams
	while (m_socket->hasPendingDatagrams ()) {
		QByteArray packet;
		packet.resize (m_socket->pendingDatagramSize ());
		QHostAddress address;

		// check for minimal protocol length
		if (m_socket->readDatagram (packet.data (), packet.size (), &address) >= 2)
			handleDatagram (address, packet);
	}
}

void UdpDiscovery::handleDatagram (const QHostAddress & address, const QByteArray & dg) {
	Settings settings;
	if (dg.at (0) == UDP_TYPE_PING) {
		// check if it is not local
		if (address != QHostAddress::LocalHost && address != QHostAddress::LocalHostIPv6) {
			// emit answer
			QByteArray packet;
			packet.push_back ((quint8) UDP_TYPE_ANSWER);
			packet.push_back ((quint8) dg.at (1));

			// add name to answer
			QString name = settings.name ();
			packet.push_back (name.toUtf8 ());

			// send
			m_socket->writeDatagram (packet, address, m_port);
		}
	} else if (dg.at (0) == UDP_TYPE_ANSWER) {
		// extract info
		quint8 seqNum = dg.at (1);
		QString name = QString::fromUtf8 (dg.mid (2));

		// check if this answer if acceptable
		if (seqNum == m_sequenceNumber && name != settings.name ()) {
			Peer newPeer (name, address);
			
			// check if not already got
			if (not m_peerList.contains (newPeer)) {
				// then add to list and send update signal
				m_peerList.push_back (newPeer);
				emit updateList (m_peerList);
			}
		}
	}
}

