/*
 * Program wide general declarations
 */
#ifndef LOCALSHARE_H
#define LOCALSHARE_H

namespace Const {
constexpr auto app_name = "localshare";

// Network related
constexpr int name_size_limit = 300; // Length of username

constexpr int discovery_port = 41563;   // Discovery system port
constexpr int dicovery_keep_alive = 60; // Time between keep alive packets (s)

constexpr int default_tcp_port = discovery_port + 1;

// Graphical
constexpr int drag_icon_size = 32; // Size of icons attached to cursor during drag&drop operations
}

#endif
