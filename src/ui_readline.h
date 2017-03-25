/*
Copyright (c) 2008-2011
	Lars-Dominik Braun <lars@6xq.net>

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

#pragma once

#include "config.h"

#include <stdbool.h>
#include <stdlib.h>

typedef enum {
	BAR_RL_DEFAULT = 0,
	BAR_RL_FULLRETURN = 1, /* return if buffer is full */
	BAR_RL_NOECHO = 2, /* don't echo to stdout */
} BarReadlineFlags_t;

typedef struct _BarReadline_t *BarReadline_t;

void BarReadlineInit(BarReadline_t*);
void BarReadlineDestroy(BarReadline_t);
size_t BarReadline (char *, const size_t, const char *,
		BarReadline_t, const BarReadlineFlags_t, int);
size_t BarReadlineStr (char *, const size_t,
		BarReadline_t, const BarReadlineFlags_t);
size_t BarReadlineInt (int *, BarReadline_t);
bool BarReadlineYesNo (bool, BarReadline_t);

