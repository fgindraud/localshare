/* Localshare - Small file sharing application for the local network.
 * Copyright (C) 2016 Francois Gindraud
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
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
void always_print (const QString & msg);
void error_print (const QString & msg);
void exit_nicely (void);
void exit_error (void);

int start (int & argc, char **& argv);
}

#endif
