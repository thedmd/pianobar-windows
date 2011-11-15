/*
Copyright (c) 2008-2011
	Lars-Dominik Braun <lars@6xq.net>
Copyright (c) 2011
	Micha³ Cichoñ <michcic@gmail.com>

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "config.h"
#include "terminal.h"
#include "ui_readline.h"

#ifdef _WIN32

static INLINE char* BarReadlineNextUtf8 (char* ptr) {
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

static INLINE char* BarReadlinePriorUtf8 (char* ptr) {
	while ((*(--ptr) & 0xC0) == 0x80)
		/*continue*/;

	return ptr;
}

static INLINE int BarReadlineEncodeUtf8 (int codePoint, char* utf8) {
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


/*	readline replacement
 *	@param buffer
 *	@param buffer size
 *	@param accept these characters
 *	@param input fds
 *	@param flags
 *	@param timeout (seconds) or -1 (no timeout)
 *	@return number of bytes read from stdin
 */
size_t BarReadline (char *buf, const size_t bufSize, const char *mask,
		BarReadlineFds_t *input, const BarReadlineFlags_t flags, int timeout)
{
	HANDLE handle = BarConsoleGetStdInHandleWin32 ();
	DWORD timeStamp, waitResult;
	COORD cursorPosition;
	int bufPos = 0, bufLen = 0;
	char* bufOut = buf;

	const bool password = flags & BAR_RL_PASSWORD;
	const bool overflow = flags & BAR_RL_FULLRETURN;
	const bool echo     = !(flags & BAR_RL_NOECHO) && !password;

	assert (buf != NULL);
	assert (bufSize > 0);
	assert (input != NULL);

	memset (buf, 0, bufSize);

	if (timeout != INFINITE) {
		// convert timeout to ms
		timeout *= 1000;

		// get time stamp, required for simulating non-locking input timeouts
		timeStamp = GetTickCount();
	}
	else
      timeStamp = 0;

	if (echo)
		cursorPosition = BarConsoleGetCursorPositionWin32 ();

	while (true) {
		if (timeout != INFINITE) {
			DWORD now = GetTickCount ();
			if ((int)(now - timeStamp) < timeout) {
				timeout -= (int)(now - timeStamp);
				timeStamp = now;
			}
			else
				timeout = 0;
		}

		waitResult = WaitForSingleObject (handle, timeout);

		if (WAIT_OBJECT_0 == waitResult) {
			INPUT_RECORD inputRecords[8];
			INPUT_RECORD* record;
			DWORD recordsRead, i;

			ReadConsoleInput (handle, inputRecords, sizeof(inputRecords) / sizeof(*inputRecords), &recordsRead);

			for (i = 0, record = inputRecords; i < recordsRead; ++i, ++record) {
				int codePoint, keyCode;

				if ((record->EventType != KEY_EVENT) || !record->Event.KeyEvent.bKeyDown)
					continue;

				keyCode   = record->Event.KeyEvent.wVirtualKeyCode;
				codePoint = record->Event.KeyEvent.uChar.UnicodeChar;

				switch (keyCode) {
					case VK_LEFT:
						if (bufPos > 0) {
							if (echo) {
								BarConsoleMoveCursorWin32 (-1);
							}
							bufOut = BarReadlinePriorUtf8 (bufOut);
							--bufPos;
						}
						break;

					case VK_RIGHT:
						if (bufPos < bufLen) {
							if (echo) {
								BarConsoleMoveCursorWin32 (1);
							}
							bufOut = BarReadlineNextUtf8 (bufOut);
							++bufPos;
						}
						break;

					case VK_HOME:
						if (echo) {
							BarConsoleSetCursorPositionWin32 (cursorPosition);
						}
						bufPos = 0;
						bufOut = buf;
						break;

					case VK_END:
						if (echo) {
							BarConsoleSetCursorPositionWin32 (cursorPosition);
							BarConsoleMoveCursorWin32 (bufLen);
						}
						bufPos = bufLen;
						bufOut = buf + strlen (buf);
						break;

					case VK_BACK:
						if (bufPos > 0) {
							int moveSize;

							char* oldBufOut = bufOut;
							bufOut = BarReadlinePriorUtf8 (bufOut);
							moveSize = strlen(bufOut) - (oldBufOut - bufOut);
							memmove (bufOut, oldBufOut, moveSize);
							bufOut[moveSize] = '\0';

							if (echo) {
								BarConsoleMoveCursorWin32 (-1);
							}

							--bufPos;
							--bufLen;
						}
						break;

					case VK_DELETE:
						if (bufPos < bufLen) {
							int moveSize;

							char* nextCharOut = BarReadlineNextUtf8 (bufOut);
							moveSize = strlen (bufOut) - (bufOut - nextCharOut);
							memmove (bufOut, nextCharOut, moveSize);
							bufOut[moveSize] = '\0';

							--bufLen;
						}
						break;

					case VK_RETURN:
						if (echo || password)
							fputc ('\n', stdout);
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
						if (mask != NULL && (codePoint > 255 || !strchr (mask, (char)codePoint)))
							break;

						encodedCodePointLength = BarReadlineEncodeUtf8 (codePoint, encodedCodePoint);
						encodedCodePoint[encodedCodePointLength] = '\0';

						if (bufLen + encodedCodePointLength < (int)bufSize) {
							strncpy (bufOut, encodedCodePoint, encodedCodePointLength);
							bufOut += encodedCodePointLength;
							++bufPos;
							++bufLen;

							if ((bufLen >= (int)(bufSize - 1)) && overflow) {
								if (echo || password)
									fputc ('\n', stdout);
								return bufLen;
							}
						}
					}
					break;
				}

				if (echo) {
					COORD currentCursorPosition = cursorPosition;
					currentCursorPosition.X += bufPos;

					BarConsoleSetCursorPositionWin32 (cursorPosition);
					fputs (buf, stdout);
					fputc (' ', stdout);
					BarConsoleSetCursorPositionWin32 (currentCursorPosition);
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

#else /* _WIN32 */

static INLINE void BarReadlineMoveLeft (char *buf, size_t *bufPos,
		size_t *bufLen) {
	char *tmpBuf = &buf[*bufPos-1];
	while (tmpBuf < &buf[*bufLen]) {
		*tmpBuf = *(tmpBuf+1);
		++tmpBuf;
	}
	--(*bufPos);
	--(*bufLen);
}

static INLINE char BarReadlineIsAscii (char b) {
	return !(b & (1 << 7));
}

static INLINE char BarReadlineIsUtf8Start (char b) {
	return (b & (1 << 7)) && (b & (1 << 6));
}

static INLINE char BarReadlineIsUtf8Content (char b) {
	return (b & (1 << 7)) && !(b & (1 << 6));
}

/*	readline replacement
 *	@param buffer
 *	@param buffer size
 *	@param accept these characters
 *	@param input fds
 *	@param flags
 *	@param timeout (seconds) or -1 (no timeout)
 *	@return number of bytes read from stdin
 */
size_t BarReadline (char *buf, const size_t bufSize, const char *mask,
		BarReadlineFds_t *input, const BarReadlineFlags_t flags, int timeout) {
	size_t bufPos = 0;
	size_t bufLen = 0;
	unsigned char escapeState = 0;
	fd_set set;
	const bool password = !!(flags & BAR_RL_PASSWORD);
	const bool echo     = !(flags & BAR_RL_NOECHO) && !password;

	assert (buf != NULL);
	assert (bufSize > 0);
	assert (input != NULL);

	memset (buf, 0, bufSize);

	/* if fd is a fifo fgetc will always return EOF if nobody writes to
	 * it, stdin will block */
	while (1) {
		int curFd = -1;
		unsigned char chr;
		struct timeval timeoutstruct;

		/* select modifies set and timeout */
		memcpy (&set, &input->set, sizeof (set));
		timeoutstruct.tv_sec = timeout;
		timeoutstruct.tv_usec = 0;

		if (select (input->maxfd, &set, NULL, NULL,
				(timeout == -1) ? NULL : &timeoutstruct) <= 0) {
			/* fail or timeout */
			break;
		}

		assert (sizeof (input->fds) / sizeof (*input->fds) == 2);
		if (FD_ISSET(input->fds[0], &set)) {
			curFd = input->fds[0];
		} else if (input->fds[1] != -1 && FD_ISSET(input->fds[1], &set)) {
			curFd = input->fds[1];
		}
		if (read (curFd, &chr, sizeof (chr)) <= 0) {
			/* select() is going wild if fdset contains EOFed stdin, only check
			 * for stdin, fifo is "reopened" as soon as another writer is
			 * available
			 * FIXME: ugly */
			if (curFd == STDIN_FILENO) {
				FD_CLR (curFd, &input->set);
			}
			continue;
		}
		switch (chr) {
			/* EOT */
			case 4:
				if (echo || password) {
					fputs ("\n", stdout);
				}
				return bufLen;
				break;

			/* return */
			case 10:
				if (echo || password) {
					fputs ("\n", stdout);
				}
				return bufLen;
				break;

			/* escape */
			case 27:
				escapeState = 1;
				break;

			/* del */
			case 126:
				break;

			/* backspace */
			case 8: /* ASCII BS */
			case 127: /* ASCII DEL */
				if (bufPos > 0) {
					if (BarReadlineIsAscii (buf[bufPos-1])) {
						BarReadlineMoveLeft (buf, &bufPos, &bufLen);
					} else {
						/* delete utf-8 multibyte chars */
						/* char content */
						while (BarReadlineIsUtf8Content (buf[bufPos-1])) {
							BarReadlineMoveLeft (buf, &bufPos, &bufLen);
						}
						/* char length */
						if (BarReadlineIsUtf8Start (buf[bufPos-1])) {
							BarReadlineMoveLeft (buf, &bufPos, &bufLen);
						}
					}
					/* move caret back and delete last character */
					if (echo) {
						fputs ("\033[D\033[K", stdout);
						fflush (stdout);
					}
				} else if (bufPos == 0 && buf[bufPos] != '\0') {
					/* delete char at position 0 but don't move cursor any further */
					buf[bufPos] = '\0';
					if (echo) {
						fputs ("\033[K", stdout);
						fflush (stdout);
					}
				}
				break;

			default:
				/* ignore control/escape characters */
				if (chr <= 0x1F) {
					break;
				}
				if (escapeState == 2) {
					escapeState = 0;
					break;
				}
				if (escapeState == 1 && chr == '[') {
					escapeState = 2;
					break;
				}
				/* don't accept chars not in mask */
				if (mask != NULL && !strchr (mask, chr)) {
					break;
				}
				/* don't write beyond buffer's limits */
				if (bufPos < bufSize-1) {
					buf[bufPos] = chr;
					++bufPos;
					++bufLen;
					if (echo) {
						putchar (chr);
						fflush (stdout);
					}
					/* buffer full => return if requested */
					if (bufPos >= bufSize-1 && (flags & BAR_RL_FULLRETURN)) {
						if (echo || password) {
							fputs ("\n", stdout);
						}
						return bufLen;
					}
				}
				break;
		} /* end switch */
	} /* end while */
	return 0;
}

#endif /* _WIN32 */

/*	Read string from stdin
 *	@param buffer
 *	@param buffer size
 *	@return number of bytes read from stdin
 */
size_t BarReadlineStr (char *buf, const size_t bufSize,
		BarReadlineFds_t *input, const BarReadlineFlags_t flags) {
	return BarReadline (buf, bufSize, NULL, input, flags, -1);
}

/*	Read int from stdin
 *	@param write result into this variable
 *	@return number of bytes read from stdin
 */
size_t BarReadlineInt (int *ret, BarReadlineFds_t *input) {
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
bool BarReadlineYesNo (bool def, BarReadlineFds_t *input) {
	char buf[2];
	BarReadline (buf, sizeof (buf), "yYnN", input, BAR_RL_FULLRETURN, -1);
	if (*buf == 'y' || *buf == 'Y' || (def == true && *buf == '\0')) {
		return true;
	} else {
		return false;
	}
}
