/*
Copyright (c) 2008-2013
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

#include "config.h"

#include <assert.h>
#include <piano.h>

#include "main.h"
#include "console.h"
#include "ui.h"
#include "ui_dispatch.h"
#include "ui_readline.h"

/*	authenticate user
 */
static bool BarMainLoginUser(BarApp_t *app)
{
    PianoReturn_t pRet;
    PianoRequestDataLogin_t reqData;
    bool ret;

    reqData.user = app->settings.username;
    reqData.password = app->settings.password;
    reqData.step = 0;

    BarUiMsg(&app->settings, MSG_INFO, "Login... ");
    ret = BarUiPianoCall(app, PIANO_REQUEST_LOGIN, &reqData, &pRet);
    BarUiStartEventCmd(&app->settings, "userlogin", NULL, NULL, &app->player,
        NULL, pRet);

    return ret;
}

/*	ask for username/password if none were provided in settings
 */
static bool BarMainGetLoginCredentials(BarSettings_t *settings,
    BarReadline_t rl)
{
    bool usernameFromConfig = true;

    if (settings->username == NULL)
    {
        char nameBuf[100];

        BarUiMsg(settings, MSG_QUESTION, "Email: ");
        if (BarReadlineStr(nameBuf, sizeof(nameBuf), rl, BAR_RL_DEFAULT) == 0)
            return false;
        settings->username = strdup(nameBuf);
        usernameFromConfig = false;
    }

    if (settings->password == NULL)
    {
        char passBuf[100];

        if (usernameFromConfig)
        {
            BarUiMsg(settings, MSG_QUESTION, "Email: %s\n", settings->username);
        }

		if (settings->passwordCmd == NULL) {
			BarUiMsg (settings, MSG_QUESTION, "Password: ");
			if (BarReadlineStr (passBuf, sizeof (passBuf), rl, BAR_RL_NOECHO) == 0) {
                BarConsolePutc('\n');
				return false;
			}
            /* write missing newline */
            BarConsolePutc('\n');
			settings->password = strdup (passBuf);
		}
        else
        {
			//pid_t chld;
			//int pipeFd[2];

            //BarUiMsg (settings, MSG_INFO, "Requesting password from external helper... ");

            //if (pipe (pipeFd) == -1) {
            //	BarUiMsg (settings, MSG_NONE, "Error: %s\n", strerror (errno));
            //	return false;
            //}

            //chld = fork ();
            //if (chld == 0) {
            //	/* child */
            //	close (pipeFd[0]);
            //	dup2 (pipeFd[1], fileno (stdout));
            //	execl ("/bin/sh", "/bin/sh", "-c", settings->passwordCmd, (char *) NULL);
            //	BarUiMsg (settings, MSG_NONE, "Error: %s\n", strerror (errno));
            //	close (pipeFd[1]);
            //	exit (1);
            //} else if (chld == -1) {
            //	BarUiMsg (settings, MSG_NONE, "Error: %s\n", strerror (errno));
            //	return false;
            //} else {
            //	/* parent */
            //	int status;

            //	close (pipeFd[1]);
            //	memset (passBuf, 0, sizeof (passBuf));
            //	read (pipeFd[0], passBuf, sizeof (passBuf)-1);
            //	close (pipeFd[0]);

            //	/* drop trailing newlines */
            //	ssize_t len = strlen (passBuf)-1;
            //	while (len >= 0 && passBuf[len] == '\n') {
            //		passBuf[len] = '\0';
            //		--len;
            //	}

            //	waitpid (chld, &status, 0);
            //	if (WEXITSTATUS (status) == 0) {
            //		settings->password = strdup (passBuf);
            //		BarUiMsg (settings, MSG_NONE, "Ok.\n");
            //	} else {
            //		BarUiMsg (settings, MSG_NONE, "Error: Exit status %i.\n", WEXITSTATUS (status));
            //		return false;
            //	}
            //}
            return false;
        } /* end else passwordCmd */
    }

    return true;
}

/*	get station list
 */
static bool BarMainGetStations(BarApp_t *app)
{
    PianoReturn_t pRet;
    bool ret;

    BarUiMsg(&app->settings, MSG_INFO, "Get stations... ");
    ret = BarUiPianoCall(app, PIANO_REQUEST_GET_STATIONS, NULL, &pRet);
    BarUiStartEventCmd(&app->settings, "usergetstations", NULL, NULL, &app->player,
        app->ph.stations, pRet);
    return ret;
}

/*	get initial station from autostart setting or user rl
 */
static void BarMainGetInitialStation (BarApp_t *app) {
	/* try to get autostart station */
	if (app->settings.autostartStation != NULL) {
		app->nextStation = PianoFindStationById (app->ph.stations,
				app->settings.autostartStation);
		if (app->nextStation == NULL) {
			BarUiMsg (&app->settings, MSG_ERR,
					"Error: Autostart station not found.\n");
		}
	}
	/* no autostart? ask the user */
	if (app->nextStation == NULL) {
		app->nextStation = BarUiSelectStation (app, app->ph.stations,
				"Select station: ", NULL, app->settings.autoselect);
	}
}

/*	wait for user rl
 */
static void BarMainHandleUserInput(BarApp_t *app)
{
    char buf[2];
    if (BarReadline(buf, sizeof(buf), NULL, app->rl,
        BAR_RL_FULLRETURN | BAR_RL_NOECHO, 1) > 0)
    {
        BarUiDispatch(app, buf[0], app->curStation, app->playlist, true,
            BAR_DC_GLOBAL);
    }
}

/*	fetch new playlist
 */
static void BarMainGetPlaylist (BarApp_t *app) {
	PianoReturn_t pRet;
	PianoRequestDataGetPlaylist_t reqData;
	reqData.station = app->nextStation;
	reqData.quality = app->settings.audioQuality;

	BarUiMsg (&app->settings, MSG_INFO, "Receiving new playlist... ");
	if (!BarUiPianoCall (app, PIANO_REQUEST_GET_PLAYLIST,
			&reqData, &pRet)) {
		app->nextStation = NULL;
	} else {
		app->playlist = reqData.retPlaylist;
		if (app->playlist == NULL) {
			BarUiMsg (&app->settings, MSG_INFO, "No tracks left.\n");
			app->nextStation = NULL;
		}
	}
	app->curStation = app->nextStation;
	BarUiStartEventCmd (&app->settings, "stationfetchplaylist",
			app->curStation, app->playlist, &app->player, app->ph.stations,
			pRet);
}

/*	start new player thread
 */
static void BarMainStartPlayback(BarApp_t *app)
{
    assert(app != NULL);

    const PianoSong_t * const curSong = app->playlist;
    assert(curSong != NULL);

    BarUiPrintSong(&app->settings, curSong, app->curStation->isQuickMix ?
        PianoFindStationById(app->ph.stations,
            curSong->stationId) : NULL);

    static const char httpPrefix[] = "http://";
    /* avoid playing local files */
    if (curSong->audioUrl == NULL ||
        strncmp(curSong->audioUrl, httpPrefix, strlen(httpPrefix)) != 0)
    {
        BarUiMsg(&app->settings, MSG_ERR, "Invalid song url.\n");
    }
    else
    {
        BarPlayer2SetGain(app->player, curSong->fileGain);
        BarPlayer2Open(app->player, curSong->audioUrl);

        /* throw event */
        BarUiStartEventCmd(&app->settings, "songstart",
            app->curStation, curSong, &app->player, app->ph.stations,
            PIANO_RET_OK);

        if (!BarPlayer2Play(app->player))
            ++app->playerErrors;
        else
            app->playerErrors = 0;
    }
}

/*	player is done, clean up
 */
static void BarMainPlayerCleanup(BarApp_t *app)
{
    BarUiStartEventCmd(&app->settings, "songfinish", app->curStation,
        app->playlist, &app->player, app->ph.stations, PIANO_RET_OK);

    BarPlayer2Finish(app->player);

    BarConsoleSetTitle(TITLE);

    if (app->playerErrors >= app->settings.maxPlayerErrors)
    {
        /* don't continue playback if thread reports too many error */
        app->nextStation = NULL;
        app->playerErrors = 0;
    }
}

/*	print song duration
 */
static void BarMainPrintTime(BarApp_t *app)
{
    double songPlayed, songDuration, songRemaining;
    char sign;

    songDuration = BarPlayer2GetDuration(app->player);
    songPlayed = BarPlayer2GetTime(app->player);

    if (songPlayed <= songDuration)
    {
        songRemaining = songDuration - songPlayed;
        sign = '-';
    }
    else
    {
        /* longer than expected */
        songRemaining = songPlayed - songDuration;
        sign = '+';
    }
    BarUiMsg(&app->settings, MSG_TIME, "%c%02u:%02u/%02u:%02u\r",
        sign, (int)songRemaining / 60, (int)songRemaining % 60,
        (int)songDuration / 60, (int)songDuration % 60);
}

/*	main loop
 */
static void BarMainLoop(BarApp_t *app)
{
    if (!BarMainGetLoginCredentials(&app->settings, app->rl))
    {
        return;
    }

    if (!BarMainLoginUser(app))
    {
        return;
    }

    if (!BarMainGetStations(app))
    {
        return;
    }

    BarMainGetInitialStation(app);

    while (!app->doQuit)
    {
        /* song finished playing, clean up things/scrobble song */
        if (BarPlayer2IsStopped(app->player))
        {
            BarMainPlayerCleanup(app);
        }

        /* check whether player finished playing and start playing new
         * song */
        if (BarPlayer2IsFinished(app->player) && app->nextStation != NULL)
        {
            /* what's next? */
            if (app->playlist != NULL)
            {
                PianoSong_t *histsong = app->playlist;
                app->playlist = PianoListNextP(app->playlist);
                histsong->head.next = NULL;
                BarUiHistoryPrepend(app, histsong);
            }
			if (app->playlist == NULL && app->nextStation != NULL && !app->doQuit)
			{
				if (app->nextStation != app->curStation)
				{
					BarUiPrintStation (&app->settings, app->nextStation);
				}
				BarMainGetPlaylist (app);
			}
            /* song ready to play */
            if (app->playlist != NULL)
            {
                BarMainStartPlayback(app);
            }
        }

        BarMainHandleUserInput(app);

        /* show time */
        if (BarPlayer2IsPlaying(app->player) || BarPlayer2IsPaused(app->player))
        {
            BarMainPrintTime(app);
        }
    }
}

int main(int argc, char **argv)
{
    static BarApp_t app;

    memset(&app, 0, sizeof(app));

    BarConsoleInit();

    BarConsoleSetTitle(TITLE);

    /* init some things */
    BarSettingsInit(&app.settings);
    BarSettingsRead(&app.settings);

    if (!BarPlayer2Init(&app.player, app.settings.player))
    {
        if (app.settings.player)
            BarUiMsg(&app.settings, MSG_ERR, "Player \"%s\" initialization failed.", app.settings.player);
        else
            BarUiMsg(&app.settings, MSG_ERR, "Player initialization failed.");
        return 0;
    }

    PianoReturn_t pret;
    if ((pret = PianoInit(&app.ph, app.settings.partnerUser,
        app.settings.partnerPassword, app.settings.device,
        app.settings.inkey, app.settings.outkey)) != PIANO_RET_OK)
    {
        BarUiMsg(&app.settings, MSG_ERR, "Initialization failed:"
            " %s\n", PianoErrorToStr(pret));
        return 0;
    }

    BarUiMsg(&app.settings, MSG_NONE,
        "Welcome to " PACKAGE " (" VERSION ")!\n");
    if (app.settings.keys[BAR_KS_HELP] == BAR_KS_DISABLED)
    {
        BarUiMsg(&app.settings, MSG_NONE, "\n");
    }
    else
    {
        BarUiMsg(&app.settings, MSG_NONE,
            "Press %c for a list of commands.\n",
            app.settings.keys[BAR_KS_HELP]);
    }

    HttpInit(&app.http2, app.settings.rpcHost, app.settings.rpcTlsPort);
    if (app.settings.controlProxy)
        HttpSetProxy(app.http2, app.settings.controlProxy);


    BarReadlineInit(&app.rl);

    BarMainLoop(&app);

    BarReadlineDestroy(app.rl);

    /* write statefile */
    BarSettingsWrite(app.curStation, &app.settings);

    PianoDestroy(&app.ph);
    PianoDestroyPlaylist(app.songHistory);
    PianoDestroyPlaylist(app.playlist);
    HttpDestroy(app.http2);
    BarPlayer2Destroy(app.player);
    BarSettingsDestroy(&app.settings);
    BarConsoleDestroy();

    return 0;
}

