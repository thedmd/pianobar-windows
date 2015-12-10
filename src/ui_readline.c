/*
Copyright (c) 2008-2014
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

#include "ui_readline.h"
#include "console.h"
#include <stdio.h>
#include <assert.h>

static inline char* BarReadlineNextUtf8 (char* ptr) {
    if ((*ptr & 0x80) == 0)
        return ptr + 1;
    else if ((*ptr & 0xE0) == 0xC0)
        return ptr + 2;
    else if ((*ptr & 0xF0) == 0xE0)
        return ptr + 3;
    else if ((*ptr & 0xF8) == 0xF0)
        return ptr + 4;
    else
        return ptr;
}

static inline char* BarReadlinePriorUtf8 (char* ptr) {
    while ((*(--ptr) & 0xC0) == 0x80)
        /*continue*/;

    return ptr;
}

static inline int BarReadlineEncodeUtf8 (int codePoint, char* utf8) {
    if (codePoint < 0x80) {
        utf8[0] = (char)codePoint;
        return 1;
    }
    else if (codePoint < 0x0800) {
        utf8[0] = (char)(0xC0 | ((codePoint >> 6) & 0x1F));
        utf8[1] = (char)(0x80 | ((codePoint >> 0) & 0x3F));
        return 2;
    }
    else if (codePoint < 0x10000) {
        utf8[0] = (char)(0xE0 | ((codePoint >> 12) & 0x0F));
        utf8[1] = (char)(0x80 | ((codePoint >>  6) & 0x3F));
        utf8[2] = (char)(0x80 | ((codePoint >>  0) & 0x3F));
        return 3;
    }
    else if (codePoint < 0x110000) {
        utf8[0] = (char)(0xF0 | ((codePoint >> 18) & 0x07));
        utf8[1] = (char)(0x80 | ((codePoint >> 12) & 0x3F));
        utf8[2] = (char)(0x80 | ((codePoint >>  6) & 0x3F));
        utf8[2] = (char)(0x80 | ((codePoint >>  0) & 0x3F));
        return 4;
    }
    else
        return 0;
}

struct _BarReadline_t {
	DWORD  DefaultAttr;
};

void BarReadlineInit(BarReadline_t* rl) {
	static struct _BarReadline_t instance;
	*rl = &instance;
}

void BarReadlineDestroy(BarReadline_t rl) {
}

/*	return size of previous UTF-8 character
 */
static size_t BarReadlinePrevUtf8 (char *ptr) {
	size_t i = 0;

	do {
		++i;
		--ptr;
	} while ((*ptr & (1 << 7)) && !(*ptr & (1 << 6)));

	return i;
}

/*	readline replacement
 *	@param buffer
 *	@param buffer size
 *	@param accept these characters
 *	@param readline
 *	@param flags
 *	@param timeout (seconds) or -1 (no timeout)
 *	@return number of bytes read from stdin
 */
size_t BarReadline (char *buf, const size_t bufSize, const char *mask,
		BarReadline_t input, const BarReadlineFlags_t flags, int timeout) {
	HANDLE handle = BarConsoleGetStdIn();
	DWORD timeStamp, waitResult;
	int bufPos = 0, bufLen = 0;
	char* bufOut = buf;

	const bool overflow = flags & BAR_RL_FULLRETURN;
	const bool echo     = !(flags & BAR_RL_NOECHO);

	assert(buf != NULL);
	assert(bufSize > 0);
	assert(input != NULL);

	memset(buf, 0, bufSize);

	if (timeout != INFINITE) {
		// convert timeout to ms
		timeout *= 1000;

		// get time stamp, required for simulating non-locking input timeouts
		timeStamp = GetTickCount();
	}
	else
		timeStamp = 0;

	while (true) {
		if (timeout != INFINITE) {
			DWORD now = GetTickCount();
			if ((int)(now - timeStamp) < timeout) {
				timeout -= (int)(now - timeStamp);
				timeStamp = now;
			}
			else
				timeout = 0;
		}

		waitResult = WaitForSingleObject(handle, timeout);

		if (WAIT_OBJECT_0 == waitResult) {
			INPUT_RECORD inputRecords[8];
			INPUT_RECORD* record;
			DWORD recordsRead, i;

			ReadConsoleInput(handle, inputRecords, sizeof(inputRecords) / sizeof(*inputRecords), &recordsRead);

			for (i = 0, record = inputRecords; i < recordsRead; ++i, ++record)
			{
				int codePoint, keyCode;

				if ((record->EventType != KEY_EVENT) || !record->Event.KeyEvent.bKeyDown)
					continue;

				keyCode   = record->Event.KeyEvent.wVirtualKeyCode;
				codePoint = record->Event.KeyEvent.uChar.UnicodeChar;

				switch (keyCode) {
					case VK_LEFT:
						if (bufPos > 0)
						{
							if (echo) {
								BarConsoleMoveCursor(-1);
							}
							bufOut = BarReadlinePriorUtf8(bufOut);
							--bufPos;
						}
						break;

					case VK_RIGHT:
						if (bufPos < bufLen)
						{
							if (echo) {
								BarConsoleMoveCursor(1);
							}
							bufOut = BarReadlineNextUtf8(bufOut);
							++bufPos;
						}
						break;

					case VK_HOME:
						if (echo) {
							BarConsoleMoveCursor(-bufPos);
						}
						bufPos = 0;
						bufOut = buf;
						break;

					case VK_END:
						if (echo) {
							BarConsoleMoveCursor(bufLen - bufPos);
						}
						bufPos = bufLen;
						bufOut = buf + strlen(buf);
						break;

					case VK_BACK:
						if (bufPos > 0) {
							int moveSize;

							char* oldBufOut = bufOut;
							bufOut = BarReadlinePriorUtf8(bufOut);
							moveSize = strlen(bufOut) - (oldBufOut - bufOut);
							memmove(bufOut, oldBufOut, moveSize);
							bufOut[moveSize] = '\0';

							if (echo) {
								BarConsoleMoveCursor(-1);
								BarConsoleEraseCharacter();
							}

							--bufPos;
							--bufLen;
						}
						break;

					case VK_DELETE:
						if (bufPos < bufLen) {
							int moveSize;

							if (echo) {
								BarConsoleEraseCharacter();
							}

							char* nextCharOut = BarReadlineNextUtf8(bufOut);
							moveSize = strlen(bufOut) - (bufOut - nextCharOut);
							memmove(bufOut, nextCharOut, moveSize);
							bufOut[moveSize] = '\0';

							--bufLen;
						}
						break;

					case VK_RETURN:
						if (echo)
                            BarConsolePutc('\n');
						return bufLen;

					default: {
							char encodedCodePoint[5];
							int encodedCodePointLength;

							/*
							if (keyCode == VK_MEDIA_PLAY_PAUSE) {
							codePoint = 'p';
							PlaySoundA("SystemNotification", NULL, SND_ASYNC);
							}
							else if (keyCode == VK_MEDIA_NEXT_TRACK) {
							codePoint = 'n';
							PlaySoundA("SystemNotification", NULL, SND_ASYNC);
							}
							*/

							if (codePoint <= 0x1F)
								break;

							/* don't accept chars not in mask */
							if (mask != NULL && (codePoint > 255 || !strchr(mask, (char)codePoint)))
								break;

							encodedCodePointLength = BarReadlineEncodeUtf8(codePoint, encodedCodePoint);
							encodedCodePoint[encodedCodePointLength] = '\0';

							if (bufLen + encodedCodePointLength < (int)bufSize)
							{
								strncpy(bufOut, encodedCodePoint, encodedCodePointLength);

								if (echo) {
                                    BarConsolePuts(encodedCodePoint);
									BarConsoleFlush();
								}

								bufOut += encodedCodePointLength;
								++bufPos;
								++bufLen;

								if ((bufLen >= (int)(bufSize - 1)) && overflow)
								{
									if (echo)
                                        BarConsolePutc('\n');
									return bufLen;
								}
							}
						}
						break;
				}
			}
		}
		else if (WAIT_TIMEOUT == waitResult)
			break;
		else
			/* TODO: Handle errors. */
			break;
	}

	return bufLen;
}

/*	Read string from stdin
 *	@param buffer
 *	@param buffer size
 *	@return number of bytes read from stdin
 */
size_t BarReadlineStr (char *buf, const size_t bufSize,
		BarReadline_t input, const BarReadlineFlags_t flags) {
	return BarReadline (buf, bufSize, NULL, input, flags, -1);
}

/*	Read int from stdin
 *	@param write result into this variable
 *	@return number of bytes read from stdin
 */
size_t BarReadlineInt (int *ret, BarReadline_t input) {
	int rlRet = 0;
	char buf[16];

	rlRet = BarReadline (buf, sizeof (buf), "0123456789", input,
			BAR_RL_DEFAULT, -1);
	*ret = atoi ((char *) buf);

	return rlRet;
}

/*	Yes/No?
 *	@param default (user presses enter)
 */
bool BarReadlineYesNo (bool def, BarReadline_t input) {
	char buf[2];
	BarReadline (buf, sizeof (buf), "yYnN", input, BAR_RL_FULLRETURN, -1);
	if (*buf == 'y' || *buf == 'Y' || (def == true && *buf == '\0')) {
		return true;
	} else {
		return false;
	}
}

