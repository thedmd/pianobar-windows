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

#include "console.h"
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include "vtparse/vtparse.h"

# define BAR_BUFFER_CAPACITY    1024

static const int BarTerminalToAttibColor[8] = {
    0 /* black */,  4 /* red */,      2 /* green  */,   6 /* yellow */,
    1 /* blue  */,  5 /* magenta */,  3 /* cyan */,     7 /* white */
};

static struct BarConsoleState
{
    HANDLE StdIn;
    HANDLE StdOut;

    WORD DefaultAttributes;
    WORD CurrentAttributes;

    HANDLE ConsoleThread;
    bool Terminate;

    vtparse_t Parser;

    char   Buffer[BAR_BUFFER_CAPACITY];
    size_t BufferSize;
} g_BarConsole;

static inline void BarOutSetAttributes(WORD attributes)
{
    g_BarConsole.CurrentAttributes = attributes;
    SetConsoleTextAttribute(BarConsoleGetStdOut(), g_BarConsole.CurrentAttributes);
}

static void BarParseCallback(struct vtparse* parser, vtparse_action_t action, unsigned char ch)
{
    if (action == VTPARSE_ACTION_PRINT || action == VTPARSE_ACTION_EXECUTE)
    {
        putc(ch, stdout);
        if (action == VTPARSE_ACTION_EXECUTE)
            fflush(stdout);
    }

    if (action == VTPARSE_ACTION_CSI_DISPATCH)
    {
        WORD attribute = g_BarConsole.CurrentAttributes;
        int i;

        switch (ch)
        {
            case 'K':
                BarConsoleEraseLine(parser->num_params > 0 ? parser->params[0] : 0);
                break;

            case 'm':
                for (i = 0; i < parser->num_params; ++i)
                {
                    int p = parser->params[i];
                    if (p == 0)
                        attribute = g_BarConsole.DefaultAttributes;
                    //else if (p == 1)
                    //	attribute |= FOREGROUND_INTENSITY;
                    else if (p == 4)
                        attribute |= COMMON_LVB_UNDERSCORE;
                    else if (p == 7)
                        attribute |= COMMON_LVB_REVERSE_VIDEO;
                    //else if (p == 21)
                    //	attribute &= ~FOREGROUND_INTENSITY;
                    else if (p == 24)
                        attribute &= ~COMMON_LVB_UNDERSCORE;
                    else if (p == 27)
                        attribute &= ~COMMON_LVB_REVERSE_VIDEO;
                    else if (p >= 30 && p <= 37)
                        attribute = (attribute & ~0x07) | BarTerminalToAttibColor[p - 30];
                    else if (p >= 40 && p <= 47)
                        attribute = (attribute & ~0x70) | (BarTerminalToAttibColor[p - 40] << 4);
                    else if (p >= 90 && p <= 97)
                        attribute = (attribute & ~0x07) | BarTerminalToAttibColor[p - 90] | FOREGROUND_INTENSITY;
                    else if (p >= 100 && p <= 107)
                        attribute = ((attribute & ~0x70) | (BarTerminalToAttibColor[p - 100] << 4)) | BACKGROUND_INTENSITY;
                }
                fflush(stdout);
                BarOutSetAttributes(attribute);
                break;
        }
    }
}

void BarConsoleInit()
{
    unsigned threadId = 0;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    memset(&g_BarConsole, 0, sizeof(g_BarConsole));

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    g_BarConsole.StdIn  = GetStdHandle(STD_INPUT_HANDLE);
    g_BarConsole.StdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleScreenBufferInfo(g_BarConsole.StdOut, &csbi);

    g_BarConsole.DefaultAttributes = csbi.wAttributes;
    g_BarConsole.CurrentAttributes = csbi.wAttributes;

    vtparse_init(&g_BarConsole.Parser, BarParseCallback);
}

void BarConsoleDestroy()
{
}

HANDLE BarConsoleGetStdIn()
{
    return g_BarConsole.StdIn;
}

HANDLE BarConsoleGetStdOut()
{
    return g_BarConsole.StdOut;
}

void BarConsoleSetTitle(const char* title)
{
    size_t len = MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);

    TCHAR* wTitle = malloc((len + 1) * sizeof(TCHAR));
    if (NULL != wTitle)
    {
        MultiByteToWideChar(CP_UTF8, 0, title, -1, wTitle, (int)len);
        SetConsoleTitleW(wTitle);

        free(wTitle);
    }
    else
        SetConsoleTitleA(title);
}

void BarConsoleSetSize(int width, int height)
{
    HANDLE handle;
    SMALL_RECT r;
    COORD c, s;
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    handle = BarConsoleGetStdOut();

    if (!GetConsoleScreenBufferInfo(handle, &csbi))
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

    SetConsoleScreenBufferSize(handle, c);

    r.Left = 0;
    r.Top = 0;
    r.Right = c.X - 1;
    r.Bottom = c.Y - 1;
    SetConsoleWindowInfo(handle, TRUE, &r);
}

void BarConsoleSetCursorPosition(COORD position)
{
    SetConsoleCursorPosition(BarConsoleGetStdOut(), position);
}

COORD BarConsoleGetCursorPosition()
{
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    COORD result = { -1, -1 };

    if (GetConsoleScreenBufferInfo(BarConsoleGetStdOut(), &consoleInfo))
        result = consoleInfo.dwCursorPosition;

    return result;
}

COORD BarConsoleMoveCursor(int xoffset)
{
    COORD position;

    position = BarConsoleGetCursorPosition();
    position.X += xoffset;
    BarConsoleSetCursorPosition(position);

    return position;
}

void BarConsoleEraseCharacter()
{
    TCHAR buffer[256];
    WORD buffer2[256];
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
    COORD coords;
    HANDLE handle = BarConsoleGetStdOut();
    int length, read, write;

    if (!GetConsoleScreenBufferInfo(handle, &csbiInfo))
        return;

    length = csbiInfo.dwSize.X - csbiInfo.dwCursorPosition.X - 1;
    read = csbiInfo.dwCursorPosition.X + 1;
    write = csbiInfo.dwCursorPosition.X;

    while (length >= 0)
    {
        int size = min(length, 256);
        DWORD chRead = 0, chWritten = 0;
        coords = csbiInfo.dwCursorPosition;
        coords.X = read;
        ReadConsoleOutputAttribute(handle, buffer2, size, coords, &chRead);
        ReadConsoleOutputCharacter(handle, buffer, size, coords, &chRead);

        if (chRead == 0)
            break;

        coords.X = write;
        WriteConsoleOutputAttribute(handle, buffer2, chRead, coords, &chWritten);
        WriteConsoleOutputCharacter(handle, buffer, chRead, coords, &chWritten);

        read += chRead;
        write += chRead;
        length -= chRead;
    }
}

void BarConsoleEraseLine(int mode)
{
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
    DWORD writen, length;
    COORD coords;

    HANDLE handle = BarConsoleGetStdOut();

    if (!GetConsoleScreenBufferInfo(handle, &csbiInfo))
        return;

    writen = 0;
    coords.X = 0;
    coords.Y = csbiInfo.dwCursorPosition.Y;

    switch (mode)
    {
        default:
        case 0: /* from cursor */
            coords.X = BarConsoleGetCursorPosition().X;
            length = csbiInfo.dwSize.X - coords.X;
            break;

        case 1: /* to cursor */
            coords.X = 0;
            length = BarConsoleGetCursorPosition().X;
            break;

        case 2: /* whole line */
            coords.X = 0;
            length = csbiInfo.dwSize.X;
            break;
    }

    FillConsoleOutputCharacter(handle, ' ', length, coords, &writen);
    FillConsoleOutputAttribute(handle, csbiInfo.wAttributes, csbiInfo.dwSize.X, coords, &writen);

    SetConsoleCursorPosition(handle, coords);
}

void BarConsoleSetClipboard(const char* text)
{
    WCHAR* wideString;
    HANDLE stringHandle;
    size_t wideSize, wideBytes;

    wideSize = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wideBytes = wideSize * sizeof(WCHAR);
    wideString = malloc(wideBytes);
    if (!wideString)
        return;

    MultiByteToWideChar(CP_UTF8, 0, text, -1, wideString, (int)wideSize);

    stringHandle = GlobalAlloc(GMEM_MOVEABLE, wideBytes);
    if (!stringHandle)
    {
        free(wideString);
        return;
    }

    memcpy(GlobalLock(stringHandle), wideString, wideBytes);

    GlobalUnlock(stringHandle);

    if (!OpenClipboard(NULL))
    {
        GlobalFree(stringHandle);
        free(wideString);
        return;
    }

    EmptyClipboard();

    SetClipboardData(CF_UNICODETEXT, stringHandle);

    CloseClipboard();

    free(wideString);
}

void BarConsoleFlush()
{
    char buffer[BAR_BUFFER_CAPACITY];
    size_t bufferSize = 0;

    if (g_BarConsole.BufferSize == 0)
        return;

    bufferSize = g_BarConsole.BufferSize;
    memcpy(buffer, g_BarConsole.Buffer, bufferSize);

    g_BarConsole.BufferSize = 0;

    vtparse(&g_BarConsole.Parser, buffer, (int)bufferSize);
}

void BarConsolePutc(char c)
{
    if (g_BarConsole.BufferSize >= BAR_BUFFER_CAPACITY)
        BarConsoleFlush();

    g_BarConsole.Buffer[g_BarConsole.BufferSize++] = c;
}

void BarConsolePuts(const char* c)
{
    size_t length = strlen(c);
    size_t i;

    for (i = 0; i < length; ++i)
        BarConsolePutc(c[i]);
}

static char* BarConsoleFormat(char* buffer, size_t buffer_size, const char* format, va_list args)
{
    bool allocated = false;

    int chars_writen;
    while ((chars_writen = _vsnprintf(buffer, buffer_size - 1, format, args)) < 0)
    {
        size_t new_buffer_size = buffer_size * 3 / 2;
        if (new_buffer_size < buffer_size)
        {   // handle overflow
            chars_writen = (int)buffer_size;
            break;
        }

        if (allocated)
            buffer = realloc(buffer, new_buffer_size);
        else
            buffer = malloc(new_buffer_size);
        buffer_size = new_buffer_size;
    }

    return buffer;
}

void BarConsolePrint(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    BarConsolePrintV(format, args);
    va_end(args);
}

void BarConsolePrintV(const char* format, va_list args)
{
    char localBuffer[BAR_BUFFER_CAPACITY];
    size_t bufferSize = BAR_BUFFER_CAPACITY, i = 0;

    char* buffer = BarConsoleFormat(localBuffer, bufferSize, format, args);

    BarConsolePuts(buffer);

    if (buffer != localBuffer)
        free(buffer);
}