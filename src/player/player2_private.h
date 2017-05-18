#pragma once

#include "config.h"
#include <stdbool.h>
#include "player2.h"

typedef struct _player2_iface
{
    const char*     Id;
    const char*     Name;
    player2_t     (*Create)        ();
    void          (*Destroy)       (player2_t player);
    void          (*SetVolume)     (player2_t player, float volume);
    float         (*GetVolume)     (player2_t player);
    void          (*SetGain)       (player2_t player, float gainDb);
    float         (*GetGain)       (player2_t player);
    double        (*GetDuration)   (player2_t player);
    double        (*GetTime)       (player2_t player);
    bool          (*Open)          (player2_t player, const char* url);
    bool          (*Play)          (player2_t player);
    bool          (*Pause)         (player2_t player);
    bool          (*Stop)          (player2_t player);
    bool          (*Finish)        (player2_t player);
    bool          (*IsPlaying)     (player2_t player);
    bool          (*IsPaused)      (player2_t player);
    bool          (*IsStopped)     (player2_t player);
    bool          (*IsFinished)    (player2_t player);
} player2_iface;

extern player2_iface player2_direct_show;
extern player2_iface player2_windows_media_foundation;

