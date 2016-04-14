#ifndef LOCALSHARE_H
#define LOCALSHARE_H

#include <dns_sd.h>

namespace Const {
constexpr auto app_name = "localshare";
constexpr auto service_name = "_localshare._tcp.";

// Network related
constexpr int name_size_limit = kDNSServiceMaxServiceName - 1; // Length of username

// Graphical
constexpr int drag_icon_size = 32; // Size of icons attached to cursor during drag&drop operations
}

#endif
