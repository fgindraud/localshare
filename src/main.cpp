#include "cli/main.h"
#include "gui/main.h"

#include "core/transfer.h"

namespace Transfer {
Serialized serialized_info;
}

int main (int argc, char * argv[]) {
	if (is_console_mode (argc, argv))
		return console_main (argc, argv);
	else
		return gui_main (argc, argv);
}
