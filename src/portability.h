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
#ifndef PORTABILITY_H
#define PORTABILITY_H

// Abstracts os specific stuff in a portable way

// Terminal size
#ifdef Q_OS_UNIX
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

int terminal_width (void) {
	int size = 80; // Default
#ifdef Q_OS_UNIX
	struct winsize sz;
	if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &sz) != -1)
		size = sz.ws_col;
#endif
#ifdef Q_OS_WIN
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo (GetStdHandle (STD_OUTPUT_HANDLE), &csbi))
		size = csbi.srWindow.Right - csbi.srWindow.Left;
#endif
	return size;
}

#endif
