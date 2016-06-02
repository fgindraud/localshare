#pragma once
#ifndef CLI_MAIN_H
#define CLI_MAIN_H

#include <QString>

namespace Indicator {
class Item;
}

namespace Cli {
void draw_progress_indicator (const Indicator::Item &);

void verbose_print (const QString & msg);
void normal_print (const QString & msg);
void error_print (const QString & msg);
void exit_nicely (void);
void exit_error (void);

int start (int & argc, char **& argv);
}

#endif
