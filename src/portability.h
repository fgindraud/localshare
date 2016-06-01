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
