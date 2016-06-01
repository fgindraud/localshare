#include <QtGlobal>
#include <QByteArray>

#include "cli_main.h"
#include "gui_main.h"

#include "core_transfer.h"
namespace Transfer {
Serialized serialized_info;
}

/* Determine if we are in cli mode.
 * We cannot use the nice Qt parser before a Q*Application is built.
 * Thus we manually detect options that are definite clues of a cli mode.
 * This MUST be updated if cli options change.
 */
static inline bool is_console_mode (int argc, const char * const * argv) {
	static const char * trigger_console_mode[] = {
	    "-d", "--download", "-u", "--upload", "-h", "--help", "-V", "--version", nullptr};
	for (int i = 1; i < argc; ++i)
		for (int j = 0; trigger_console_mode[j] != nullptr; ++j)
			if (qstrncmp (argv[i], trigger_console_mode[j], qstrlen (trigger_console_mode[j])) == 0)
				return true;
	return false;
}

int main (int argc, char * argv[]) {
#ifdef Q_OS_LINUX
	// Remove useless message from avahi
	qputenv ("AVAHI_COMPAT_NOWARN", "1");
#endif
	if (is_console_mode (argc, argv)) {
		return Cli::main (argc, argv);
	} else {
		return gui_main (argc, argv);
	}
}
