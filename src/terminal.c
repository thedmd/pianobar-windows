/*
Copyright (c) 2008-2010
	Lars-Dominik Braun <lars@6xq.net>
Copyright (c) 2011
	Micha³ Cichoñ <michcic@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef __FreeBSD__
#define _POSIX_C_SOURCE 1 /* fileno() */
#define _BSD_SOURCE /* setlinebuf() */
#define _DARWIN_C_SOURCE /* setlinebuf() on OS X */
#endif

#include "terminal.h"

#ifdef _WIN32

HANDLE BarConsoleGetStdInHandleWin32 (void) {
	return GetStdHandle (STD_INPUT_HANDLE);
}

HANDLE BarConsoleGetStdOutHandleWin32 (void) {
	return GetStdHandle (STD_OUTPUT_HANDLE);
}

void BarConsoleSetCursorPositionWin32 (COORD position) {
	SetConsoleCursorPosition (BarConsoleGetStdOutHandleWin32 (), position);
}

COORD BarConsoleGetCursorPositionWin32 (void) {
	CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	COORD result = { -1, -1 };

	if (GetConsoleScreenBufferInfo (BarConsoleGetStdOutHandleWin32 (), &consoleInfo))
		result = consoleInfo.dwCursorPosition;


	return result;
}

COORD BarConsoleMoveCursorWin32 (int offset) {
	COORD position;

	position = BarConsoleGetCursorPositionWin32 ();
	position.X += offset;
	BarConsoleSetCursorPositionWin32 (position);

	return position;
}

void BarConsoleClearLineWin32 (void) {
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
	DWORD writen;
	COORD coords;

	HANDLE handle = BarConsoleGetStdInHandleWin32 ();

	if (!GetConsoleScreenBufferInfo (handle, &csbiInfo))
		return;

	writen = 0;
	coords.X = 0;
	coords.Y = csbiInfo.dwCursorPosition.Y;
	FillConsoleOutputCharacter (handle, ' ', csbiInfo.dwSize.X, coords, &writen);
	FillConsoleOutputAttribute (handle, csbiInfo.wAttributes, csbiInfo.dwSize.X, coords, &writen);

	SetConsoleCursorPosition (handle, coords);
}

void BarConsoleSetSizeWin32 (int width, int height) {
	HANDLE handle;
	SMALL_RECT r;
	COORD c, s;
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	handle = BarConsoleGetStdOutHandleWin32 ();

	if (!GetConsoleScreenBufferInfo (handle, &csbi))
		return;

	s.X = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	s.Y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

	if (s.X > width && s.Y > height)
		return;

	c.X = width;
	c.Y = height;

	if (s.X > c.X)
		c.X = s.X;
	if (s.Y > c.Y)
		c.Y = s.Y;

	SetConsoleScreenBufferSize (handle, c);

	r.Left   = 0;
	r.Top    = 0;
	r.Right  = c.X - 1;
	r.Bottom = c.Y - 1;
	SetConsoleWindowInfo (handle, TRUE, &r);
}

void BarConsoleSetTitle (const char* title) {
	size_t len = MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);

	TCHAR* wTitle = malloc ((len + 1) * sizeof (TCHAR));
	if (NULL != wTitle)
	{
		MultiByteToWideChar(CP_UTF8, 0, title, -1, wTitle, len);
		SetConsoleTitleW (wTitle);
		
		free(wTitle);
	}
	else
		SetConsoleTitleA (title);
}

void BarConsoleInitialize (void) {
	SetConsoleCP (CP_UTF8);
	SetConsoleOutputCP (CP_UTF8);
}


#else /* _WIN32 */

#include <termios.h>
#include <stdio.h>

/*	en/disable echoing for stdin
 *	@param 1 = enable, everything else = disable
 */
void BarTermSetEcho (char enable) {
	struct termios termopts;

	tcgetattr (fileno (stdin), &termopts);
	if (enable == 1) {
		termopts.c_lflag |= ECHO;
	} else {
		termopts.c_lflag &= ~ECHO;
	}
	tcsetattr(fileno (stdin), TCSANOW, &termopts);
}

/*	en/disable stdin buffering; when enabling line-buffer method will be
 *	selected for you
 *	@param 1 = enable, everything else = disable
 */
void BarTermSetBuffer (char enable) {
	struct termios termopts;

	tcgetattr (fileno (stdin), &termopts);
	if (enable == 1) {
		termopts.c_lflag |= ICANON;
		setlinebuf (stdin);
	} else {
		termopts.c_lflag &= ~ICANON;
		setvbuf (stdin, NULL, _IONBF, 1);
	}
	tcsetattr(fileno (stdin), TCSANOW, &termopts);
}

/*	Save old terminal settings
 *	@param save settings here
 */
void BarTermSave (struct termios *termOrig) {
	tcgetattr (fileno (stdin), termOrig);
}

/*	Restore terminal settings
 *	@param Old settings
 */
void BarTermRestore (struct termios *termOrig) {
	tcsetattr (fileno (stdin), TCSANOW, termOrig);
}

#endif /* _WIN32 */
