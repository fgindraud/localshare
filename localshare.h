#ifndef LOCALSHARE_H
#define LOCALSHARE_H

#include <QDataStream>
#include <QHostAddress>
#include <QDebug>

namespace Const {
// Application name for settings, etc...
constexpr auto app_name = "localshare";

// Network service name
constexpr auto service_name = "_localshare._tcp.";

// Protocol
constexpr auto serializer_version = QDataStream::Qt_5_0; // We are only compatible with Qt5 anyway
constexpr quint16 protocol_version = 1;
constexpr quint16 protocol_magic = 0x0CAA;

// Graphical
constexpr int drag_icon_size = 32; // Size of icons attached to cursor during drag&drop operations
}

/*
 * Peer information
 */
struct Peer {
	QString username;
	QString hostname;
	QHostAddress address;
	quint16 port; // Stored in host byte order
};

inline QDebug operator<<(QDebug debug, const Peer & peer) {
	QDebugStateSaver saver (debug);
	debug.nospace () << "Peer(" << peer.username << ", " << peer.hostname << ", " << peer.address
	                 << ", " << peer.port << ")";
	return debug;
}

#endif
