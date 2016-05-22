#ifndef LOCALSHARE_H
#define LOCALSHARE_H

#include <QDataStream>
#include <QHostAddress>

namespace Const {
// Application name for settings, etc...
constexpr auto app_name = "localshare";
constexpr auto app_display_name = "Localshare";

#define XSTR(x) #x
#define STR(x) XSTR (x)
constexpr auto app_version = STR (LOCALSHARE_VERSION);
#undef STR
#undef XSTR

// Network service name
constexpr auto service_type = "_localshare._tcp.";

// Protocol
constexpr auto serializer_version = QDataStream::Qt_5_0; // We are only compatible with Qt5 anyway
constexpr quint16 protocol_version = 1;
constexpr quint16 protocol_magic = 0x0CAA;
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

#endif
