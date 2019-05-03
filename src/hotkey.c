/*
Copyright (c) 2019
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

#include "hotkey.h"
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>

static BarHotKey_t* g_HotKeys = NULL;
static int g_HotKeyCapacity = 0;
static int g_HotKeyCount = 0;

void BarHotKeyInit ()
{
}

void BarHotKeyDestroy ()
{
}

void BarHotKeyPool (BarHotKeyHandler handler, void * userData)
{
    MSG msg = {0};
    while (PeekMessage(&msg, NULL, WM_HOTKEY, WM_HOTKEY, PM_REMOVE) != 0)
    {
        int id = (int)msg.wParam;
        if (id < 0)
            continue;

        handler(id, userData);
    }
}

bool BarHotKeyRegister (BarHotKey_t hk)
{
    UINT modifiers = 0;
    SHORT mappedVirtualKey = VkKeyScanA(tolower(hk.key));
    BYTE keyCode = LOBYTE(mappedVirtualKey);
    if (keyCode == 0)
        return false;

    if (hk.mods & BAR_HK_MOD_SHIFT)
        modifiers |= MOD_SHIFT;
    if (hk.mods & BAR_HK_MOD_ALT)
        modifiers |= MOD_ALT;
    if (hk.mods & BAR_HK_MOD_CTRL)
        modifiers |= MOD_CONTROL;
    if (hk.mods & BAR_HK_MOD_WIN)
        modifiers |= MOD_WIN;

    modifiers |= /*MOD_NOREPEAT*/0x4000;

    return RegisterHotKey(NULL, hk.id, modifiers, keyCode);
}

void BarHotKeyUnregister (int id)
{
    UnregisterHotKey(NULL, id);
}

bool BarHotKeyParse (BarHotKey_t* result, const char *value)
{
    BarHotKey_t parsed = { 0 };
    const char* p = value;
    size_t s = 0;

    while ((s = strcspn(p, " +")) > 0)
    {
        if ((s == 4 && strnicmp(p, "ctrl", 4) == 0) || (s == 7 && strnicmp(p, "control", 7) == 0))
            parsed.mods |= BAR_HK_MOD_CTRL;
        else if ((s == 5 && strnicmp(p, "shift", 5) == 0))
            parsed.mods |= BAR_HK_MOD_SHIFT;
        else if ((s == 3 && strnicmp(p, "alt", 3) == 0))
            parsed.mods |= BAR_HK_MOD_ALT;
        else if (s == 1)
            parsed.key = *p;
        else
            return false;

        p += s;
        p += strspn(p, " +");
    }

    if (parsed.key == 0)
        return false;

    result->key = parsed.key;
    result->mods = parsed.mods;

    return true;
}