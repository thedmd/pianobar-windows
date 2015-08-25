/*
Copyright (c) 2015
	Micha³ Cichoñ <thedmd@interia.pl>

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

#ifndef SRC_CONSOLE_H_WY8F3MNH
#define SRC_CONSOLE_H_WY8F3MNH

#include "config.h"

#include <windows.h>

void BarConsoleInit ();
void BarConsoleDestroy ();
HANDLE BarConsoleGetStdIn ();
HANDLE BarConsoleGetStdOut ();
void BarConsoleSetTitle (const char* title);
void BarConsoleSetSize (int width, int height);
void BarConsoleSetCursorPosition (COORD position);
COORD BarConsoleGetCursorPosition ();
COORD BarConsoleMoveCursor (int xoffset);
void BarConsoleEraseCharacter ();
void BarConsoleEraseLine (int mode); // 0 - from cursor, 1 - to cursor, 2 - entire line

void BarConsoleSetClipboard (const char*);

#endif /* SRC_CONSOLE_H_WY8F3MNH */
