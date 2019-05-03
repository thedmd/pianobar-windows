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

# pragma once

#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

typedef enum {
    BAR_HK_MOD_NONE = 0,
    BAR_HK_MOD_SHIFT = 1,
    BAR_HK_MOD_ALT = 2,
    BAR_HK_MOD_CTRL = 4,
    BAR_HK_MOD_WIN = 8
} BarHotKeyMods_t;

typedef struct {
    int id;
    char key;
    BarHotKeyMods_t mods;
} BarHotKey_t;

typedef void (*BarHotKeyHandler)(int, void *);

void BarHotKeyInit ();
void BarHotKeyDestroy ();
void BarHotKeyPool (BarHotKeyHandler, void *);
bool BarHotKeyRegister (BarHotKey_t);
void BarHotKeyUnregister (int);
bool BarHotKeyParse (BarHotKey_t* result, const char* value);

