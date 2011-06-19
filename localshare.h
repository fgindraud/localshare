#ifndef H_DECL
#define H_DECL

#define APP_NAME "LocalShare"

#define NAME_SIZE_LIMIT (0x1 << 8)

#define DEFAULT_UDP_PORT 41563
#define DEFAULT_TCP_PORT (DEFAULT_UDP_PORT + 1)

#define UDP_TYPE_PING 0x1
#define UDP_TYPE_ANSWER 0x2

class TrayIcon;
class SendWindow;
class PeerListWidget;

#endif

