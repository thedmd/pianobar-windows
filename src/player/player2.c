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

/* receive/play audio stream */

/* based on DShow example player */

#include "config.h"
#include "player2_private.h"
#include <stdlib.h>
#include <memory.h>

# define length_of(x)   (sizeof(x)/sizeof(*(x)))

static player2_iface* player2_backends[] =
{
    &player2_direct_show
};

struct _player_t
{
    player2_iface*  backend;
    player2_t       player;
};

bool BarPlayer2Init(player2_t* outPlayer)
{
    player2_t player;
    struct _player_t result;
    int i;

    memset(&result, 0, sizeof(struct _player_t));

    for (i = 0; i < length_of(player2_backends); ++i)
    {
        player2_iface* backend = player2_backends[i];

        result.player = backend->Create();
        if (result.player)
        {
            result.backend = backend;
            break;
        }
    }

    if (!result.backend)
        return false;

    player = malloc(sizeof(struct _player_t));
    if (!player)
        return false;

    *player = result;

    *outPlayer = player;

    return true;
}

void BarPlayer2Destroy(player2_t player)
{
    if (player->player)
    {
        player->backend->Destroy(player->player);
        player->player = NULL;
    }
}

void BarPlayer2SetVolume(player2_t player, float volume)
{
    if (player->player)
        player->backend->SetVolume(player->player, volume);
}

float BarPlayer2GetVolume(player2_t player)
{
    if (player->player)
        return player->backend->GetVolume(player->player);
    else
        return 0.0f;
}

void BarPlayer2SetGain(player2_t player, float gainDb)
{
    if (player->player)
        player->backend->SetGain(player->player, gainDb);
}

float BarPlayer2GetGain(player2_t player)
{
    if (player->player)
        return player->backend->GetGain(player->player);
    else
        return 0.0f;
}

double BarPlayer2GetDuration(player2_t player)
{
    if (player->player)
        return player->backend->GetDuration(player->player);
    else
        return 0.0f;
}

double BarPlayer2GetTime(player2_t player)
{
    if (player->player)
        return player->backend->GetTime(player->player);
    else
        return 0.0f;
}

bool BarPlayer2Open(player2_t player, const char* url)
{
    if (player->player)
        return player->backend->Open(player->player, url);
    else
        return false;
}

bool BarPlayer2Play(player2_t player)
{
    if (player->player)
        return player->backend->Play(player->player);
    else
        return false;
}

bool BarPlayer2Pause(player2_t player)
{
    if (player->player)
        return player->backend->Pause(player->player);
    else
        return false;
}

bool BarPlayer2Stop(player2_t player)
{
    if (player->player)
        return player->backend->Stop(player->player);
    else
        return false;
}

bool BarPlayer2Finish(player2_t player)
{
    if (player->player)
        return player->backend->Finish(player->player);
    else
        return false;
}

bool BarPlayer2IsPlaying(player2_t player)
{
    if (player->player)
        return player->backend->IsPlaying(player->player);
    else
        return false;
}

bool BarPlayer2IsPaused(player2_t player)
{
    if (player->player)
        return player->backend->IsPaused(player->player);
    else
        return false;
}

bool BarPlayer2IsStopped(player2_t player)
{
    if (player->player)
        return player->backend->IsStopped(player->player);
    else
        return false;
}

bool BarPlayer2IsFinished(player2_t player)
{
    if (player->player)
        return player->backend->IsFinished(player->player);
    else
        return true;
}