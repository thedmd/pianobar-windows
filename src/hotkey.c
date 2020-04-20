/*
Copyright (c) 2019
    Michał Cichoń <thedmd@interia.pl>

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

typedef struct BarVirtualKeyMap
{
    const char* name;
    UINT vk;
} BarVirtualKeyMap;

static BarHotKey_t* g_HotKeys = NULL;
static int g_HotKeyCapacity = 0;
static int g_HotKeyCount = 0;
static const BarVirtualKeyMap g_VirtualKeyMap[] =
{
    { "NUMPAD0", VK_NUMPAD0 },
    { "NUMPAD1", VK_NUMPAD1 },
    { "NUMPAD2", VK_NUMPAD2 },
    { "NUMPAD3", VK_NUMPAD3 },
    { "NUMPAD4", VK_NUMPAD4 },
    { "NUMPAD5", VK_NUMPAD5 },
    { "NUMPAD6", VK_NUMPAD6 },
    { "NUMPAD7", VK_NUMPAD7 },
    { "NUMPAD8", VK_NUMPAD8 },
    { "NUMPAD9", VK_NUMPAD9 },
    { "MULTIPLY", VK_MULTIPLY },
    { "ADD", VK_ADD },
    { "SEPARATOR", VK_SEPARATOR },
    { "SUBTRACT", VK_SUBTRACT },
    { "DECIMAL", VK_DECIMAL },
    { "DIVIDE", VK_DIVIDE },
    { "F1", VK_F1 },
    { "F2", VK_F2 },
    { "F3", VK_F3 },
    { "F4", VK_F4 },
    { "F5", VK_F5 },
    { "F6", VK_F6 },
    { "F7", VK_F7 },
    { "F8", VK_F8 },
    { "F9", VK_F9 },
    { "F10", VK_F10 },
    { "F11", VK_F11 },
    { "F12", VK_F12 },
    { "F13", VK_F13 },
    { "F14", VK_F14 },
    { "F15", VK_F15 },
    { "F16", VK_F16 },
    { "F17", VK_F17 },
    { "F18", VK_F18 },
    { "F19", VK_F19 },
    { "F20", VK_F20 },
    { "F21", VK_F21 },
    { "F22", VK_F22 },
    { "F23", VK_F23 },
    { "F24", VK_F24 },
    { "BROWSER_BACK", VK_BROWSER_BACK },
    { "BROWSER_FORWARD", VK_BROWSER_FORWARD },
    { "BROWSER_REFRESH", VK_BROWSER_REFRESH },
    { "BROWSER_STOP", VK_BROWSER_STOP },
    { "BROWSER_SEARCH", VK_BROWSER_SEARCH },
    { "BROWSER_FAVORITES", VK_BROWSER_FAVORITES },
    { "BROWSER_HOME", VK_BROWSER_HOME },
    { "VOLUME_MUTE", VK_VOLUME_MUTE },
    { "VOLUME_DOWN", VK_VOLUME_DOWN },
    { "VOLUME_UP", VK_VOLUME_UP },
    { "MEDIA_NEXT_TRACK", VK_MEDIA_NEXT_TRACK },
    { "MEDIA_PREV_TRACK", VK_MEDIA_PREV_TRACK },
    { "MEDIA_STOP", VK_MEDIA_STOP },
    { "MEDIA_PLAY_PAUSE", VK_MEDIA_PLAY_PAUSE },
    { "LAUNCH_MAIL", VK_LAUNCH_MAIL },
    { "LAUNCH_MEDIA_SELECT", VK_LAUNCH_MEDIA_SELECT },
    { "LAUNCH_APP1", VK_LAUNCH_APP1 },
    { "LAUNCH_APP2", VK_LAUNCH_APP2 },
};

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
    SHORT mappedVirtualKey = hk.key;
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

static UINT BarFindKeyByName(const char* name, size_t nameLength)
{
    for (int i = 0; i < sizeof(g_VirtualKeyMap) / sizeof(*g_VirtualKeyMap); ++i)
    {
        if (strlen(g_VirtualKeyMap[i].name) != nameLength)
            continue;

        if (strnicmp(g_VirtualKeyMap[i].name, name, nameLength) == 0)
            return g_VirtualKeyMap[i].vk;
    }

    return 0;
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
            parsed.key = VkKeyScanA(tolower(*p));
        else if ((parsed.key = BarFindKeyByName(p, s)) == 0)
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