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

/* application settings */

#ifndef __FreeBSD__
#define _POSIX_C_SOURCE 1 /* PATH_MAX */
#define _BSD_SOURCE /* strdup() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#include "settings.h"
#include "config.h"
#include "ui_dispatch.h"

#ifndef PATH_MAX
#define PATH_MAX 260
#endif

#define streq(a, b) (strcmp (a, b) == 0)

/*	tries to guess your config dir; somehow conforming to
 *	http://standards.freedesktop.org/basedir-spec/basedir-spec-0.6.html
 *	@param name of the config file (can contain subdirs too)
 *	@param store the whole path here
 *	@param but only up to this size
 *	@return nothing
 */
void BarGetXdgConfigDir (const char *filename, char *retDir,
		size_t retDirN) {
	char *xdgConfigDir = NULL;

	if ((xdgConfigDir = getenv ("XDG_CONFIG_HOME")) != NULL &&
			strlen (xdgConfigDir) > 0) {
		/* special dir: $xdg_config_home */
		bar_snprintf (retDir, retDirN, "%s/%s", xdgConfigDir, filename);
	} else {
		if ((xdgConfigDir = getenv ("HOME")) != NULL &&
				strlen (xdgConfigDir) > 0) {
			/* standard config dir: $home/.config */
			bar_snprintf (retDir, retDirN, "%s/.config/%s", xdgConfigDir,
					filename);
		} else {
			/* fallback: working dir */
			bar_snprintf (retDir, retDirN, "%s", filename);
		}
	}
}

/*	initialize settings structure
 *	@param settings struct
 */
void BarSettingsInit (BarSettings_t *settings) {
	memset (settings, 0, sizeof (*settings));

# ifdef _WIN32
	settings->width = 160;
	settings->height = 40;
# endif
}

/*	free settings structure, zero it afterwards
 *	@oaram pointer to struct
 */
void BarSettingsDestroy (BarSettings_t *settings) {
	size_t i;
	free (settings->controlProxy);
	free (settings->proxy);
	free (settings->username);
	free (settings->password);
	free (settings->autostartStation);
	free (settings->eventCmd);
	free (settings->loveIcon);
	free (settings->banIcon);
	free (settings->atIcon);
	free (settings->npSongFormat);
	free (settings->npStationFormat);
	free (settings->listSongFormat);
	free (settings->fifo);
	free (settings->rpcHost);
	free (settings->partnerUser);
	free (settings->partnerPassword);
	free (settings->device);
	free (settings->inkey);
	free (settings->outkey);
	for (i = 0; i < MSG_COUNT; i++) {
		free (settings->msgFormat[i].prefix);
		free (settings->msgFormat[i].postfix);
	}
	memset (settings, 0, sizeof (*settings));
}

/*	read app settings from file; format is: key = value\n
 *	@param where to save these settings
 *	@return nothing yet
 */
void BarSettingsRead (BarSettings_t *settings) {
	size_t i;
	char configfile[PATH_MAX], key[256], val[256];
	FILE *configfd;
	static const char *formatMsgPrefix = "format_msg_";

	assert (sizeof (settings->keys) / sizeof (*settings->keys) ==
			sizeof (dispatchActions) / sizeof (*dispatchActions));

	/* apply defaults */
	#ifdef ENABLE_FAAD
	settings->audioFormat = PIANO_AF_AACPLUS;
	#else
		#ifdef ENABLE_MAD
		settings->audioFormat = PIANO_AF_MP3;
		#endif
	#endif
	settings->autoselect = true;
	settings->history = 5;
	settings->volume = 0;
	settings->sortOrder = BAR_SORT_NAME_AZ;
	settings->loveIcon = bar_strdup (" <3");
	settings->banIcon = bar_strdup (" </3");
	settings->atIcon = bar_strdup (" @ ");
	settings->npSongFormat = bar_strdup ("\"%t\" by \"%a\" on \"%l\"%r%@%s");
	settings->npStationFormat = bar_strdup ("Station \"%n\" (%i)");
	settings->listSongFormat = bar_strdup ("%i) %a - %t%r");
	settings->rpcHost = bar_strdup("internal-tuner.pandora.com");
	settings->partnerUser = bar_strdup ("pandora one");
	settings->partnerPassword = bar_strdup ("TVCKIBGS9AO9TSYLNNFUML0743LH82D");
	settings->device = bar_strdup ("D01");
	settings->inkey = bar_strdup ("U#IO$RZPAB%VX2");
	settings->outkey = bar_strdup ("2%3WCL*JU$MP]4");
	settings->fifo = malloc (PATH_MAX * sizeof (*settings->fifo));
	memcpy (settings->tlsFingerprint, "\x39\x40\x67\xee\xcb\x42\x0e\x39"
			"\x11\x98\x5c\x2c\xc0\x2c\x12\xae\x89\xbb\x09\xb2",
			
			sizeof (settings->tlsFingerprint));
	#ifdef _WIN32
	strncpy (settings->fifo, "\\\\.\\pipe\\" PACKAGE "\\ctl", PATH_MAX);
	#else
	BarGetXdgConfigDir (PACKAGE "/ctl", settings->fifo, PATH_MAX);
	#endif

	settings->msgFormat[MSG_NONE].prefix = NULL;
	settings->msgFormat[MSG_NONE].postfix = NULL;
	settings->msgFormat[MSG_INFO].prefix = bar_strdup ("(i) ");
	settings->msgFormat[MSG_INFO].postfix = NULL;
	settings->msgFormat[MSG_PLAYING].prefix = bar_strdup ("|>  ");
	settings->msgFormat[MSG_PLAYING].postfix = NULL;
	settings->msgFormat[MSG_TIME].prefix = bar_strdup ("#   ");
	settings->msgFormat[MSG_TIME].postfix = NULL;
	settings->msgFormat[MSG_ERR].prefix = bar_strdup ("/!\\ ");
	settings->msgFormat[MSG_ERR].postfix = NULL;
	settings->msgFormat[MSG_QUESTION].prefix = bar_strdup ("[?] ");
	settings->msgFormat[MSG_QUESTION].postfix = NULL;
	settings->msgFormat[MSG_LIST].prefix = bar_strdup ("\t");
	settings->msgFormat[MSG_LIST].postfix = NULL;

	for (i = 0; i < BAR_KS_COUNT; i++) {
		settings->keys[i] = dispatchActions[i].defaultKey;
	}

	#ifdef _WIN32
	strncpy (configfile, PACKAGE ".cfg", sizeof (configfile));
	#else
	BarGetXdgConfigDir (PACKAGE "/config", configfile, sizeof (configfile));
	#endif
	if ((configfd = fopen (configfile, "r")) == NULL) {
		return;
	}

	/* read config file */
	while (1) {
		char lwhite, rwhite;
		int scanRet = fscanf (configfd, "%255s%c=%c%255[^\n]", key, &lwhite, &rwhite, val);
		if (scanRet == EOF) {
			break;
		} else if (scanRet != 4 || lwhite != ' ' || rwhite != ' ') {
			/* invalid config line */
			continue;
		}
		if (streq ("control_proxy", key)) {
			settings->controlProxy = bar_strdup (val);
		} else if (streq ("proxy", key)) {
			settings->proxy = bar_strdup (val);
		} else if (streq ("user", key)) {
			settings->username = bar_strdup (val);
		} else if (streq ("password", key)) {
			settings->password = bar_strdup (val);
		} else if (streq ("rpc_host", key)) {
			free (settings->rpcHost);
			settings->rpcHost = bar_strdup (val);
		} else if (streq ("partner_user", key)) {
			free (settings->partnerUser);
			settings->partnerUser = bar_strdup (val);
		} else if (streq ("partner_password", key)) {
			free (settings->partnerPassword);
			settings->partnerPassword = bar_strdup (val);
		} else if (streq ("device", key)) {
			free (settings->device);
			settings->device = bar_strdup (val);
		} else if (streq ("encrypt_password", key)) {
			free (settings->outkey);
			settings->outkey = bar_strdup (val);
		} else if (streq ("decrypt_password", key)) {
			free (settings->inkey);
			settings->inkey = bar_strdup (val);
		} else if (memcmp ("act_", key, 4) == 0) {
			size_t i;
			/* keyboard shortcuts */
			for (i = 0; i < BAR_KS_COUNT; i++) {
				if (streq (dispatchActions[i].configKey, key)) {
					if (streq (val, "disabled")) {
						settings->keys[i] = BAR_KS_DISABLED;
					} else {
						settings->keys[i] = val[0];
					}
					break;
				}
			}
		} else if (streq ("audio_format", key)) {
			if (streq (val, "aacplus")) {
				settings->audioFormat = PIANO_AF_AACPLUS;
			} else if (streq (val, "aacplus-lofi")) {
				settings->audioFormat = PIANO_AF_AACPLUS_LO;
			} else if (streq (val, "mp3")) {
				settings->audioFormat = PIANO_AF_MP3;
			} else if (streq (val, "mp3-hifi")) {
				settings->audioFormat = PIANO_AF_MP3_HI;
			}
		} else if (streq ("autostart_station", key)) {
			settings->autostartStation = bar_strdup (val);
		} else if (streq ("event_command", key)) {
			settings->eventCmd = bar_strdup (val);
		} else if (streq ("history", key)) {
			settings->history = atoi (val);
		} else if (streq ("sort", key)) {
			size_t i;
			static const char *mapping[] = {"name_az",
					"name_za",
					"quickmix_01_name_az",
					"quickmix_01_name_za",
					"quickmix_10_name_az",
					"quickmix_10_name_za",
					};
			for (i = 0; i < BAR_SORT_COUNT; i++) {
				if (streq (mapping[i], val)) {
					settings->sortOrder = i;
					break;
				}
			}
		} else if (streq ("love_icon", key)) {
			free (settings->loveIcon);
			settings->loveIcon = bar_strdup (val);
		} else if (streq ("ban_icon", key)) {
			free (settings->banIcon);
			settings->banIcon = bar_strdup (val);
		} else if (streq ("at_icon", key)) {
			free (settings->atIcon);
			settings->atIcon = bar_strdup (val);
		} else if (streq ("volume", key)) {
			settings->volume = atoi (val);
		#ifdef _WIN32
		} else if (streq ("width", key)) {
			settings->width = atoi (val);
		} else if (streq ("height", key)) {
			settings->height = atoi (val);
		#endif
		} else if (streq ("format_nowplaying_song", key)) {
			free (settings->npSongFormat);
			settings->npSongFormat = bar_strdup (val);
		} else if (streq ("format_nowplaying_station", key)) {
			free (settings->npStationFormat);
			settings->npStationFormat = bar_strdup (val);
		} else if (streq ("format_list_song", key)) {
			free (settings->listSongFormat);
			settings->listSongFormat = bar_strdup (val);
		} else if (streq ("fifo", key)) {
			free (settings->fifo);
			settings->fifo = bar_strdup (val);
		} else if (streq ("autoselect", key)) {
			settings->autoselect = atoi (val);
		} else if (streq ("tls_fingerprint", key)) {
			/* expects 40 byte hex-encoded sha1 */
			if (strlen (val) == 40) {
				size_t i;
				for (i = 0; i < 20; i++) {
					char hex[3];
					memcpy (hex, &val[i*2], 2);
					hex[2] = '\0';
					settings->tlsFingerprint[i] = (char)strtol (hex, NULL, 16);
				}
			}
		} else if (strncmp (formatMsgPrefix, key,
				strlen (formatMsgPrefix)) == 0) {
			static const char *mapping[] = {"none", "info", "nowplaying",
					"time", "err", "question", "list"};
			const char *typeStart = key + strlen (formatMsgPrefix);
			size_t i;
			for (i = 0; i < sizeof (mapping) / sizeof (*mapping); i++) {
				if (streq (typeStart, mapping[i])) {
					const char *formatPos = strstr (val, "%s");
					
					/* keep default if there is no format character */
					if (formatPos != NULL) {
						BarMsgFormatStr_t *format = &settings->msgFormat[i];
						size_t prefixLen, postfixLen;

						free (format->prefix);
						free (format->postfix);

						prefixLen = formatPos - val;
						format->prefix = calloc (prefixLen + 1,
								sizeof (*format->prefix));
						memcpy (format->prefix, val, prefixLen);

						postfixLen = strlen (val) -
								(formatPos-val) - 2;
						format->postfix = calloc (postfixLen + 1,
								sizeof (*format->postfix));
						memcpy (format->postfix, formatPos+2, postfixLen);
					}
					break;
				}
			}
		}
	}

	/* check environment variable if proxy is not set explicitly */
	if (settings->proxy == NULL) {
		char *tmpProxy = getenv ("http_proxy");
		if (tmpProxy != NULL && strlen (tmpProxy) > 0) {
			settings->proxy = bar_strdup (tmpProxy);
		}
	}

	fclose (configfd);
}
