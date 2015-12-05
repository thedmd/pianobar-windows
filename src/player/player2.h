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

#ifndef __PIANOBAR_PLAYER2_H__
#define __PIANOBAR_PLAYER2_H__
#pragma once

#include "config.h"

#include <stdbool.h>

typedef struct _player_t *player2_t;

bool BarPlayer2Init(player2_t* outPlayer, const char* defaultPlayer);
void BarPlayer2Destroy(player2_t player);
void BarPlayer2SetVolume(player2_t player, float volume);
float BarPlayer2GetVolume(player2_t player);
void BarPlayer2SetGain(player2_t player, float gainDb);
float BarPlayer2GetGain(player2_t player);
double BarPlayer2GetDuration(player2_t player);
double BarPlayer2GetTime(player2_t player);
bool BarPlayer2Open(player2_t player, const char* url);
bool BarPlayer2Play(player2_t player);
bool BarPlayer2Pause(player2_t player);
bool BarPlayer2Stop(player2_t player);
bool BarPlayer2Finish(player2_t player);
bool BarPlayer2IsPlaying(player2_t player);
bool BarPlayer2IsPaused(player2_t player);
bool BarPlayer2IsStopped(player2_t player);
bool BarPlayer2IsFinished(player2_t player);

#endif /* __PIANOBAR_PLAYER2_H__ */

