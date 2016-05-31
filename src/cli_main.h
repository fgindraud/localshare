#pragma once
#ifndef CLI_MAIN_H
#define CLI_MAIN_H

#include <QString>

namespace Cli {
void verbose_print (const QString & msg);
void normal_print (const QString & msg);
void error_print (const QString & msg);
void exit_nicely (void);
void exit_error (void);

int main (int & argc, char **& argv);
}

#endif
