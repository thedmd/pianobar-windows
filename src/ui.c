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

/* everything that interacts with the user */

#include "config.h"

#include "ui.h"
#include "debug.h"
#include "ui_readline.h"
#include "console.h"
#include <assert.h>
#include <stdio.h>

#define streq(a, b) (strcmp (a, b) == 0)

typedef int (*BarSortFunc_t) (const void *, const void *);

/*	is string a number?
 */
static bool isnumeric (const char *s) {
	if (*s == '\0') {
		return false;
	}
	while (*s != '\0') {
		if (!isdigit ((unsigned char) *s)) {
			return false;
		}
		++s;
	}
	return true;
}

/*	find needle in haystack, ignoring case, and return first position
 */
static const char *BarStrCaseStr (const char *haystack, const char *needle) {
	const char *needlePos = needle;

	assert (haystack != NULL);
	assert (needle != NULL);

	if (*needle == '\0') {
		return haystack;
	}

	while (*haystack != '\0') {
		if (tolower ((unsigned char) *haystack) == tolower ((unsigned char) *needlePos)) {
			++needlePos;
		} else {
			needlePos = needle;
		}
		++haystack;
		if (*needlePos == '\0') {
			return haystack - strlen (needle);
		}
	}

	return NULL;
}

char* BarStrFormat (const char* format, va_list args) {
	static const size_t c_initial_buffer_size = 256;

	char* buffer = malloc(c_initial_buffer_size);
	size_t buffer_size = c_initial_buffer_size;

	int chars_writen;
	while ((chars_writen = _vsnprintf(buffer, buffer_size - 1, format, args)) < 0) {
		size_t new_buffer_size = buffer_size * 3 / 2;
		if (new_buffer_size < buffer_size) { /* handle overflow */
			chars_writen = (int)buffer_size;
			break;
		}

		buffer = realloc(buffer, new_buffer_size);
		buffer_size = new_buffer_size;
	}

	return buffer;
}

/*	output message and flush stdout
 *	@param message
 */
void BarUiMsg (const BarSettings_t *settings, const BarUiMsg_t type,
		const char *format, ...) {
	va_list fmtargs;

	assert (settings != NULL);
	assert (type < MSG_COUNT);
	assert (format != NULL);

	switch (type) {
		case MSG_INFO:
		case MSG_PLAYING:
		case MSG_TIME:
		case MSG_ERR:
		case MSG_QUESTION:
		case MSG_LIST:
		case MSG_DEBUG:
			/* print ANSI clear line */

            BarConsolePuts("\033[2K");
			break;

		default:
			break;
	}

	if (settings->msgFormat[type].prefix != NULL) {
        BarConsolePuts(settings->msgFormat[type].prefix);
	}

	va_start (fmtargs, format);
    BarConsolePrintV (format, fmtargs);

	if (type == MSG_DEBUG) {
		char* msg = BarStrFormat (format, fmtargs);
		if (msg != NULL) {
			BarConsoleSetClipboard (msg);
			free (msg);
		}
	}

	va_end (fmtargs);

	if (settings->msgFormat[type].postfix != NULL) {
        BarConsolePuts(settings->msgFormat[type].postfix);
	}

	BarConsoleFlush();
}

/*	piano wrapper: prepare/execute http request and pass result back to
 *	libpiano (updates data structures)
 *	@return 1 on success, 0 otherwise
 */
int BarUiPianoCall (BarApp_t * const app, PianoRequestType_t type,
		void *data, PianoReturn_t *pRet) {
	PianoRequest_t req;
	int netErrorRetries = 3;

	memset (&req, 0, sizeof (req));

	/* repeat as long as there are http requests to do */
	do {
		req.data = data;

		*pRet = PianoRequest (&app->ph, &req, type);
		if (*pRet != PIANO_RET_OK) {
			BarUiMsg (&app->settings, MSG_NONE, "Error: %s\n", PianoErrorToStr (*pRet));
			PianoDestroyRequest (&req);
			return 0;
		}

		if (!HttpRequest(app->http2, &req)) {
			*pRet = PIANO_RET_NETWORK_ERROR;
			BarUiMsg(&app->settings, MSG_ERR, "Network error: %s\n",
				HttpGetError(app->http2));
			if (req.responseData != NULL) {
				free(req.responseData);
			}
			if (--netErrorRetries > 0) {
				/* try again */
				*pRet = PIANO_RET_CONTINUE_REQUEST;
				BarUiMsg (&app->settings, MSG_INFO, "Trying again... ");
				continue;
			}
			PianoDestroyRequest(&req);
			return 0;
		}

		*pRet = PianoResponse (&app->ph, &req);
		if (*pRet != PIANO_RET_CONTINUE_REQUEST) {
			/* checking for request type avoids infinite loops */
			if (*pRet == PIANO_RET_P_INVALID_AUTH_TOKEN &&
					type != PIANO_REQUEST_LOGIN) {
				/* reauthenticate */
				PianoReturn_t authpRet;
				PianoRequestDataLogin_t reqData;
				reqData.user = app->settings.username;
				reqData.password = app->settings.password;
				reqData.step = 0;

				BarUiMsg (&app->settings, MSG_NONE, "Reauthentication required... ");
				if (!BarUiPianoCall (app, PIANO_REQUEST_LOGIN, &reqData, &authpRet)) {
					*pRet = authpRet;
					if (req.responseData != NULL) {
						free (req.responseData);
					}
					PianoDestroyRequest (&req);
					return 0;
				} else {
					/* try again */
					*pRet = PIANO_RET_CONTINUE_REQUEST;
					BarUiMsg (&app->settings, MSG_INFO, "Trying again... ");
				}
			} else if (*pRet != PIANO_RET_OK) {
				BarUiMsg (&app->settings, MSG_NONE, "Error: %s\n", PianoErrorToStr (*pRet));
				if (req.responseData != NULL) {
					free (req.responseData);
				}
				PianoDestroyRequest (&req);
				return 0;
			} else {
				BarUiMsg (&app->settings, MSG_NONE, "Ok.\n");
			}
		}
		/* we can destroy the request at this point, even when this call needs
		 * more than one http request. persistent data (step counter, e.g.) is
		 * stored in req.data */
		if (req.responseData != NULL) {
			free (req.responseData);
		}
		PianoDestroyRequest (&req);
	} while (*pRet == PIANO_RET_CONTINUE_REQUEST);

	return 1;
}

/*	Station sorting functions */

static inline int BarStationQuickmix01Cmp (const void *a, const void *b) {
	const PianoStation_t *stationA = *((PianoStation_t * const *) a),
			*stationB = *((PianoStation_t * const *) b);
	return stationA->isQuickMix - stationB->isQuickMix;
}

/*	sort by station name from a to z, case insensitive
 */
static inline int BarStationNameAZCmp (const void *a, const void *b) {
	const PianoStation_t *stationA = *((PianoStation_t * const *) a),
			*stationB = *((PianoStation_t * const *) b);
	return strcasecmp (stationA->name, stationB->name);
}

/*	sort by station name from z to a, case insensitive
 */
static int BarStationNameZACmp (const void *a, const void *b) {
	return BarStationNameAZCmp (b, a);
}

/*	helper for quickmix/name sorting
 */
static inline int BarStationQuickmixNameCmp (const void *a, const void *b,
		const void *c, const void *d) {
	int qmc = BarStationQuickmix01Cmp (a, b);
	return qmc == 0 ? BarStationNameAZCmp (c, d) : qmc;
}

/*	sort by quickmix (no to yes) and name (a to z)
 */
static int BarStationCmpQuickmix01NameAZ (const void *a, const void *b) {
	return BarStationQuickmixNameCmp (a, b, a, b);
}

/*	sort by quickmix (no to yes) and name (z to a)
 */
static int BarStationCmpQuickmix01NameZA (const void *a, const void *b) {
	return BarStationQuickmixNameCmp (a, b, b, a);
}

/*	sort by quickmix (yes to no) and name (a to z)
 */
static int BarStationCmpQuickmix10NameAZ (const void *a, const void *b) {
	return BarStationQuickmixNameCmp (b, a, a, b);
}

/*	sort by quickmix (yes to no) and name (z to a)
 */
static int BarStationCmpQuickmix10NameZA (const void *a, const void *b) {
	return BarStationQuickmixNameCmp (b, a, b, a);
}

/*	sort linked list (station)
 *	@param stations
 *	@return NULL-terminated array with sorted stations
 */
static PianoStation_t **BarSortedStations (PianoStation_t *unsortedStations,
		size_t *retStationCount, BarStationSorting_t order) {
	static const BarSortFunc_t orderMapping[] = {BarStationNameAZCmp,
			BarStationNameZACmp,
			BarStationCmpQuickmix01NameAZ,
			BarStationCmpQuickmix01NameZA,
			BarStationCmpQuickmix10NameAZ,
			BarStationCmpQuickmix10NameZA,
			};
	PianoStation_t **stationArray = NULL, *currStation = NULL;
	size_t stationCount = 0, i;

	assert (order < sizeof (orderMapping)/sizeof(*orderMapping));

	stationCount = PianoListCountP (unsortedStations);
	stationArray = calloc (stationCount, sizeof (*stationArray));

	/* copy station pointers */
	i = 0;
	currStation = unsortedStations;
	PianoListForeachP (currStation) {
		stationArray[i] = currStation;
		++i;
	}

	qsort (stationArray, stationCount, sizeof (*stationArray), orderMapping[order]);

	*retStationCount = stationCount;
	return stationArray;
}

/*	let user pick one station
 *	@param app handle
 *	@param stations that should be listed
 *	@param prompt string
 *	@param called if input was not a number
 *	@param auto-select if only one station remains after filtering
 *	@return pointer to selected station or NULL
 */
PianoStation_t *BarUiSelectStation (BarApp_t *app, PianoStation_t *stations,
		const char *prompt, BarUiSelectStationCallback_t callback,
		bool autoselect) {
	PianoStation_t **sortedStations = NULL, *retStation = NULL;
	size_t stationCount, i, lastDisplayed, displayCount;
	char buf[100];

	if (stations == NULL) {
		BarUiMsg (&app->settings, MSG_ERR, "No station available.\n");
		return NULL;
	}

	memset (buf, 0, sizeof (buf));

	/* sort and print stations */
	sortedStations = BarSortedStations (stations, &stationCount,
			app->settings.sortOrder);

	do {
		displayCount = 0;
		for (i = 0; i < stationCount; i++) {
			const PianoStation_t *currStation = sortedStations[i];
			/* filter stations */
			if (BarStrCaseStr (currStation->name, buf) != NULL) {
				BarUiMsg (&app->settings, MSG_LIST, "%2zi) %c%c%c %s\n", i,
						currStation->useQuickMix ? 'q' : ' ',
						currStation->isQuickMix ? 'Q' : ' ',
						!currStation->isCreator ? 'S' : ' ',
						currStation->name);
				++displayCount;
				lastDisplayed = i;
			}
		}

		BarUiMsg (&app->settings, MSG_QUESTION, "%s", prompt);
		if (autoselect && displayCount == 1 && stationCount != 1) {
			/* auto-select last remaining station */
			BarUiMsg (&app->settings, MSG_NONE, "%zi\n", lastDisplayed);
			retStation = sortedStations[lastDisplayed];
		} else {
			if (BarReadlineStr (buf, sizeof (buf), app->rl,
					BAR_RL_DEFAULT) == 0) {
				break;
			}

			if (isnumeric (buf)) {
				unsigned long selected = strtoul (buf, NULL, 0);
				if (selected < stationCount) {
					retStation = sortedStations[selected];
				}
			}

			/* hand over buffer to external function if it was not a station number */
			if (retStation == NULL && callback != NULL) {
				callback (app, buf);
			}
		}
	} while (retStation == NULL);

	free (sortedStations);
	return retStation;
}

/*	let user pick one song
 *	@param app
 *	@param song list
 *	@param input fds
 *	@return pointer to selected item in song list or NULL
 */
PianoSong_t *BarUiSelectSong (const BarApp_t * const app,
		PianoSong_t *startSong, BarReadline_t rl) {
	const BarSettings_t * const settings = &app->settings;
	PianoSong_t *tmpSong = NULL;
	char buf[100];

	memset (buf, 0, sizeof (buf));

	do {
		BarUiListSongs (app, startSong, buf);

		BarUiMsg (settings, MSG_QUESTION, "Select song: ");
		if (BarReadlineStr (buf, sizeof (buf), rl, BAR_RL_DEFAULT) == 0) {
			return NULL;
		}

		if (isnumeric (buf)) {
			unsigned long i = strtoul (buf, NULL, 0);
			tmpSong = PianoListGetP (startSong, i);
		}
	} while (tmpSong == NULL);

	return tmpSong;
}

/*	let user pick one artist
 *	@param app handle
 *	@param artists (linked list)
 *	@return pointer to selected artist or NULL on abort
 */
PianoArtist_t *BarUiSelectArtist (BarApp_t *app, PianoArtist_t *startArtist) {
	PianoArtist_t *tmpArtist = NULL;
	char buf[100];
	unsigned long i;

	memset (buf, 0, sizeof (buf));

	do {
		/* print all artists */
		i = 0;
		tmpArtist = startArtist;
		PianoListForeachP (tmpArtist) {
			if (BarStrCaseStr (tmpArtist->name, buf) != NULL) {
				BarUiMsg (&app->settings, MSG_LIST, "%2lu) %s\n", i,
						tmpArtist->name);
			}
			i++;
		}

		BarUiMsg (&app->settings, MSG_QUESTION, "Select artist: ");
		if (BarReadlineStr (buf, sizeof (buf), app->rl,
				BAR_RL_DEFAULT) == 0) {
			return NULL;
		}

		if (isnumeric (buf)) {
			i = strtoul (buf, NULL, 0);
			tmpArtist = PianoListGetP (startArtist, i);
		}
	} while (tmpArtist == NULL);

	return tmpArtist;
}

/*	search music: query, search request, return music id
 *	@param app handle
 *	@param seed suggestion station
 *	@param seed suggestion musicid
 *	@param prompt string
 *	@return musicId or NULL on abort/error
 */
char *BarUiSelectMusicId (BarApp_t *app, PianoStation_t *station,
		const char *msg) {
	char *musicId = NULL;
	char lineBuf[100], selectBuf[2];
	PianoSearchResult_t searchResult;
	PianoArtist_t *tmpArtist;
	PianoSong_t *tmpSong;

	BarUiMsg (&app->settings, MSG_QUESTION, "%s", msg);
	if (BarReadlineStr (lineBuf, sizeof (lineBuf), app->rl,
			BAR_RL_DEFAULT) > 0) {
		PianoReturn_t pRet;
		PianoRequestDataSearch_t reqData;

		reqData.searchStr = lineBuf;

		BarUiMsg (&app->settings, MSG_INFO, "Searching... ");
		if (!BarUiPianoCall (app, PIANO_REQUEST_SEARCH, &reqData, &pRet)) {
			return NULL;
		}
		memcpy (&searchResult, &reqData.searchResult, sizeof (searchResult));

		BarUiMsg (&app->settings, MSG_NONE, "\r");
		if (searchResult.songs != NULL &&
				searchResult.artists != NULL) {
			/* songs and artists found */
			BarUiMsg (&app->settings, MSG_QUESTION, "Is this an [a]rtist or [t]rack name? ");
			BarReadline (selectBuf, sizeof (selectBuf), "at", app->rl,
					BAR_RL_FULLRETURN, -1);
			if (*selectBuf == 'a') {
				tmpArtist = BarUiSelectArtist (app, searchResult.artists);
				if (tmpArtist != NULL) {
					musicId = strdup (tmpArtist->musicId);
				}
			} else if (*selectBuf == 't') {
				tmpSong = BarUiSelectSong (app, searchResult.songs,
						app->rl);
				if (tmpSong != NULL) {
					musicId = strdup (tmpSong->musicId);
				}
			}
		} else if (searchResult.songs != NULL) {
			/* songs found */
			tmpSong = BarUiSelectSong (app, searchResult.songs,
					app->rl);
			if (tmpSong != NULL) {
				musicId = strdup (tmpSong->musicId);
			}
		} else if (searchResult.artists != NULL) {
			/* artists found */
			tmpArtist = BarUiSelectArtist (app, searchResult.artists);
			if (tmpArtist != NULL) {
				musicId = strdup (tmpArtist->musicId);
			}
		} else {
			BarUiMsg (&app->settings, MSG_INFO, "Nothing found...\n");
		}
		PianoDestroySearchResult (&searchResult);
	}

	return musicId;
}

/*	replaces format characters (%x) in format string with custom strings
 *	@param destination buffer
 *	@param dest buffer size
 *	@param format string
 *	@param format characters
 *	@param replacement for each given format character
 */
void BarUiCustomFormat (char *dest, size_t destSize, const char *format,
		const char *formatChars, const char **formatVals) {
	bool haveFormatChar = false;

	while (*format != '\0' && destSize > 1) {
		if (*format == '%' && !haveFormatChar) {
			haveFormatChar = true;
		} else if (haveFormatChar) {
			const char *testChar = formatChars;
			const char *val = NULL;

			/* search for format character */
			while (*testChar != '\0') {
				if (*testChar == *format) {
					val = formatVals[(testChar-formatChars)/sizeof (*testChar)];
					break;
				}
				++testChar;
			}

			if (val != NULL) {
				/* concat */
				while (*val != '\0' && destSize > 1) {
					*dest = *val;
					++val;
					++dest;
					--destSize;
				}
			} else {
				/* invalid format character */
				*dest = '%';
				++dest;
				--destSize;
				if (destSize > 1) {
					*dest = *format;
					++dest;
					--destSize;
				}
			}

			haveFormatChar = false;
		} else {
			/* copy */
			*dest = *format;
			++dest;
			--destSize;
		}
		++format;
	}
	*dest = '\0';
}

/*	append \n to string
 */
static void BarUiAppendNewline (char *s, size_t maxlen) {
	size_t len;

	/* append \n */
	if ((len = strlen (s)) == maxlen-1) {
		s[maxlen-2] = '\n';
	} else {
		s[len] = '\n';
		s[len+1] = '\0';
	}
}

/*	Print customizeable station infos
 *	@param pianobar settings
 *	@param the station
 */
void BarUiPrintStation (const BarSettings_t *settings,
		PianoStation_t *station) {
	char outstr[512];
	const char *vals[] = {station->name, station->id};

	BarUiCustomFormat (outstr, sizeof (outstr), settings->npStationFormat,
			"ni", vals);
	BarUiAppendNewline (outstr, sizeof (outstr));
	BarUiMsg (settings, MSG_PLAYING, "%s", outstr);
}

static const char *ratingToIcon (const BarSettings_t * const settings,
		const PianoSong_t * const song) {
	switch (song->rating) {
		case PIANO_RATE_LOVE:
			return settings->loveIcon;

		case PIANO_RATE_BAN:
			return settings->banIcon;

		case PIANO_RATE_TIRED:
			return settings->tiredIcon;

		default:
			return "";
	}
}

/*	Print song infos (artist, title, album, loved)
 *	@param pianobar settings
 *	@param the song
 *	@param alternative station info (show real station for quickmix, e.g.)
 */
void BarUiPrintSong (const BarSettings_t *settings,
		const PianoSong_t *song, const PianoStation_t *station) {
	char outstr[512];
	const char *vals[] = {song->title, song->artist, song->album,
			ratingToIcon (settings, song),
			station != NULL ? settings->atIcon : "",
			station != NULL ? station->name : "",
			song->detailUrl};

	BarUiCustomFormat (outstr, sizeof (outstr), settings->npSongFormat,
			"talr@su", vals);
	BarUiAppendNewline (outstr, sizeof (outstr));
	BarUiMsg (settings, MSG_PLAYING, "%s", outstr);

	BarUiCustomFormat(outstr, sizeof(outstr), settings->titleFormat,
		"talr@su", vals);
	BarConsoleSetTitle(outstr);
}

/*	Print list of songs
 *	@param pianobar settings
 *	@param linked list of songs
 *	@param artist/song filter string
 *	@return # of songs
 */
size_t BarUiListSongs (const BarApp_t * const app,
		const PianoSong_t *song, const char *filter) {
	const BarSettings_t * const settings = &app->settings;
	size_t i = 0;

	PianoListForeachP (song) {
		if (filter == NULL ||
				(filter != NULL && (BarStrCaseStr (song->artist, filter) != NULL ||
				BarStrCaseStr (song->title, filter) != NULL))) {
			const char * const deleted = "(deleted)", * const empty = "";
			const char *stationName = empty;

			const PianoStation_t * const station =
					PianoFindStationById (app->ph.stations, song->stationId);
			if (station != NULL && station != app->curStation) {
				stationName = station->name;
			} else if (station == NULL && song->stationId != NULL) {
				stationName = deleted;
			}

			char outstr[512], digits[8], duration[8] = "??:??";
			const char *vals[] = {digits, song->artist, song->title,
					ratingToIcon (settings, song),
					duration,
					stationName != empty ? settings->atIcon : "",
					stationName,
					};

			/* pre-format a few strings */
			snprintf (digits, sizeof (digits) / sizeof (*digits), "%2zu", i);
			const unsigned int length = song->length;
			if (length > 0) {
				snprintf (duration, sizeof (duration), "%02u:%02u",
						length / 60, length % 60);
			}

			BarUiCustomFormat (outstr, sizeof (outstr), settings->listSongFormat,
					"iatrd@s", vals);
			BarUiAppendNewline (outstr, sizeof (outstr));
			BarUiMsg (settings, MSG_LIST, "%s", outstr);
		}
		i++;
	}

	return i;
}

// Function to replace a string with another 
// string 
char* replaceWord(const char* s, const char* oldW,
	const char* newW)
{
	char* result;
	int i, cnt = 0;
	int newWlen = strlen(newW);
	int oldWlen = strlen(oldW);

	// Counting the number of times old word 
	// occur in the string 
	for (i = 0; s[i] != '\0'; i++) {
		if (strstr(&s[i], oldW) == &s[i]) {
			cnt++;

			// Jumping to index after the old word. 
			i += oldWlen - 1;
		}
	}

	// Making new string of enough length 
	result = (char*)malloc(i + cnt * (newWlen - oldWlen) + 1);

	i = 0;
	while (*s) {
		// compare the substring with the result 
		if (strstr(s, oldW) == s) {
			strcpy(&result[i], newW);
			i += newWlen;
			s += oldWlen;
		}
		else
			result[i++] = *s++;
	}

	result[i] = '\0';
	return result;
}


/*	Excute external event handler
 *	@param settings containing the cmdline
 *	@param event type
 *	@param current station
 *	@param current song
 *	@param piano error-code (PIANO_RET_OK if not applicable)
 */
void BarUiStartEventCmd (const BarSettings_t *settings, const char *type,
		const PianoStation_t *curStation, const PianoSong_t *curSong,
		const player2_t * const player, PianoStation_t *stations,
		PianoReturn_t pRet) {

	if (settings->eventCmd == NULL) {
		/* nothing to do... */
		return;
	}

	if (settings->subscribedEvents == NULL) {
		/* no subscribed events... */
		return;
	}

	char* subscribedEvents = strdup(settings->subscribedEvents);
	int subscribed = 0;

	// Returns first subscribedEvent 
	char* subscribedEvent = strtok(subscribedEvents, ",");

	// Keep evaluating subscribedEvents while one of the
	// delimiters present in str[].
	while (subscribedEvent != NULL)
	{
		if (streq(type, subscribedEvent)) {
			subscribed = 1;
		}
		subscribedEvent = strtok(NULL, ",");
	}


	if (subscribed == 0) {
		BarUiMsg(settings, MSG_DEBUG, "Not subscribed to the %s event, not firing event_command.\n", type);
		return;
	} else {
		BarUiMsg(settings, MSG_DEBUG, "Subscribed to the %s event, firing event_command.\n", type);
	}

	char* replacement1 = NULL;
	char* replacement2 = NULL;
	char* replacement3 = NULL;
	char* replacement4 = NULL;
	char* cmd = NULL;

	replacement1 = replaceWord(settings->eventCmd, "$song", curSong == NULL ? "" : curSong->title);
	replacement2 = replaceWord(replacement1, "$artist", curSong == NULL ? "" : curSong->artist);
	replacement3 = replaceWord(replacement2, "$album", curSong == NULL ? "" : curSong->album);
	replacement4 = replaceWord(replacement3, "$coverArt", curSong == NULL ? "" : curSong->coverArt);
	cmd = replaceWord(replacement4, "$station", curStation == NULL ? "" : curStation->name);
	free(replacement1);
	free(replacement2);
	free(replacement3);
	free(replacement4);

	BarUiMsg(settings, MSG_DEBUG, "Raw command to run: %s\n", cmd);

	wchar_t wcmd[10000];
	mbstowcs(wcmd, cmd, 10000);

	BarUiMsg(settings, MSG_DEBUG, 
		"Variables:\nartist=%s\nalbum=%s\ntitle=%s\ncoverArt=%s\nstationName=%s\n",
		curSong == NULL ? "" : curSong->artist,
		curSong == NULL ? "" : curSong->album,
		curSong == NULL ? "" : curSong->title, 
		curSong == NULL ? "" : curSong->coverArt,
		curStation == NULL ? "" : curStation->name
	);

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// Start the child process. 
	if (!CreateProcessW(NULL,   // No module name (use command line)
		wcmd,        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi)           // Pointer to PROCESS_INFORMATION structure
		)
	{
		printf("CreateProcess failed (%d).\n", GetLastError());
		return;
	}
	free(cmd);

	// Wait until child process exits.
	//WaitForSingleObject(pi.hProcess, INFINITE);

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

/*	prepend song to history
 */
void BarUiHistoryPrepend (BarApp_t *app, PianoSong_t *song) {
	assert (app != NULL);
	assert (song != NULL);
	/* make sure it's a single song */
	assert (PianoListNextP (song) == NULL);

	if (app->settings.history != 0) {
		app->songHistory = PianoListPrependP (app->songHistory, song);
		PianoSong_t *del;
		do {
			del = PianoListGetP (app->songHistory, app->settings.history);
			if (del != NULL) {
				app->songHistory = PianoListDeleteP (app->songHistory, del);
				PianoDestroyPlaylist (del);
			} else {
				break;
			}
		} while (true);
	} else {
		PianoDestroyPlaylist (song);
	}
}

