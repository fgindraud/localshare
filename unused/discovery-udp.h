#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <QtNetwork>

#include "settings.h"
#include "localshare.h"

namespace Discovery {
struct PeerId {
	QString username;
	QHostAddress address;
};

namespace Message {

	struct Announce {
		static constexpr quint16 code = 0x1;
		QString username;
	};
	inline QDataStream & operator<<(QDataStream & out, const Announce & announce) {
		return out << announce.username;
	}
	inline QDataStream & operator>>(QDataStream & in, Announce & announce) {
		return in >> announce.username;
	}
}

class System : public QObject {
	Q_OBJECT

private:
	static constexpr auto serializer_version = QDataStream::Qt_5_6;
	static constexpr quint16 protocol_version = 1;
	static constexpr quint32 protocol_magic = 0x9EE85E7D;

	static constexpr int announce_interval_s = 20;
	static constexpr int timeout_s = 3 * announce_interval_s;

	struct Peer {
		PeerId identity;
	};

	QList<Peer> connected_peers;

	QString username;

	QUdpSocket socket;
	QTimer announce_timer;

signals:
	void new_peer (Peer);
	void peer_disconnected (Peer);

public slots:
	void change_username (QString new_name) { (void) new_name; }

public:
	// TODO use QNetworkInterface stuff to guide broadcast ?
	// TODO QNetworkConfigurationManager to tell if online or not ?
	System (QString name, QObject * parent = nullptr) : QObject (parent), username (name) {
		// Socket setup
		if (!socket.bind (QHostAddress::AnyIPv4, Const::discovery_port, QUdpSocket::ShareAddress))
			qFatal ("Cannot bind discovery socket: %s", qUtf8Printable (socket.errorString ()));
		connect (&socket, SIGNAL (readyRead ()), this, SLOT (receive_datagrams ()));

		// Start periodic announces
		connect (&announce_timer, SIGNAL (timeout ()), this, SLOT (send_announce ()));
		announce_timer.setTimerType (Qt::VeryCoarseTimer);
		announce_timer.start (announce_interval_s * 1000);
		send_announce (); // first announce
	}

private:
	template <typename MessageType>
	void send_message (const QHostAddress & to, const MessageType & msg) {
		QBuffer buffer;
		buffer.open (QBuffer::WriteOnly);
		QDataStream stream{&buffer};
		stream.setVersion (serializer_version);
		stream << protocol_magic << protocol_version << MessageType::code << msg;
		if (socket.writeDatagram (buffer.buffer (), to, Const::discovery_port) == -1)
			qDebug ("Discovery: send failed: %s", qUtf8Printable (socket.errorString ()));
	}

	template <typename MessageType>
	void process_message (const QHostAddress & from, QDataStream & stream) {
		MessageType msg;
		stream >> msg;
		if (stream.status () == QDataStream::Ok)
			process_message (from, msg);
	}

	void process_message (const QHostAddress & from, const Message::Announce & announce) {
		qDebug () << "Received announce " << announce.username << " " << from;
	}

	void process_datagram (const QHostAddress & from, QByteArray & datagram) {
		QBuffer buffer{&datagram};
		buffer.open (QBuffer::ReadOnly);
		QDataStream stream{&buffer};
		stream.setVersion (serializer_version);

		quint32 check_magic;
		quint16 check_version;
		quint16 code;
		stream >> check_magic >> check_version >> code;
		if (!(stream.status () == QDataStream::Ok && check_magic == protocol_magic &&
		      check_version == protocol_version))
			return; // ignore malformed message

		switch (code) {
		case Message::Announce::code:
			process_message<Message::Announce> (from, stream);
			break;
		default:
			break;
		}
	}

private slots:
	void receive_datagrams (void) {
		while (socket.hasPendingDatagrams ()) {
			QHostAddress sender;
			QByteArray datagram;
			datagram.resize (socket.pendingDatagramSize ());
			socket.readDatagram (datagram.data (), datagram.size (), &sender);
			process_datagram (sender, datagram);
		}
	}

	void send_announce (void) {
		qDebug () << "announce";
		send_message (QHostAddress::Broadcast, Message::Announce{username});
	}
};
}

#endif
