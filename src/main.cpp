#include <QtGlobal>

#include "cli/main.h"
#include "gui/main.h"

#include "core/transfer.h"

namespace Transfer {
Serialized serialized_info;
}

int main (int argc, char * argv[]) {
#ifdef Q_OS_LINUX
	// Remove useless message from avahi
	qputenv ("AVAHI_COMPAT_NOWARN", "1");
#endif
	if (is_console_mode (argc, argv)) {
		return console_main (argc, argv);
	} else {
		return gui_main (argc, argv);
	}
}
