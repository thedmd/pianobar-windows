/*
Copyright (c) 2008-2012
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

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "crypt.h"

#include <blowfish.h>

struct _PianoCipher_t {
	BLOWFISH_CTX cipher;
};

PianoReturn_t PianoCryptInit (PianoCipher_t* h, const char * const key,
		size_t const size) {
	PianoCipher_t result = malloc (sizeof(BLOWFISH_CTX));
	if (result == NULL) {
		return PIANO_RET_OUT_OF_MEMORY;
	}

	Blowfish_Init (&result->cipher, (unsigned char*)key, size);

	*h = result;

	return PIANO_RET_OK;
}

void PianoCryptDestroy (PianoCipher_t h) {
	free(h);
}

static inline bool PianoCryptDecrypt (PianoCipher_t h, unsigned char* output, size_t size) {
	return Blowfish_DecryptData (&h->cipher, (uint32_t*)output, (uint32_t*)output, size) == BLOWFISH_OK;
}

static inline bool PianoCryptEncrypt (PianoCipher_t h, unsigned char* output, size_t size)
{
	return Blowfish_EncryptData (&h->cipher, (uint32_t*)output, (uint32_t*)output, size) == BLOWFISH_OK;
}

/*	decrypt hex-encoded, blowfish-crypted string: decode 2 hex-encoded blocks,
 *	decrypt, byteswap
 *	@param cipher handle
 *	@param hex string
 *	@param decrypted string length (without trailing NUL)
 *	@return decrypted string or NULL
 */
char *PianoDecryptString (PianoCipher_t h, const char * const input,
		size_t * const retSize) {
	size_t inputLen = strlen (input);
	bool ret;
	unsigned char *output;
	size_t outputLen = inputLen/2;

	assert (inputLen%2 == 0);

	output = calloc (outputLen+1, sizeof (*output));
	/* hex decode */
	for (size_t i = 0; i < outputLen; i++) {
		char hex[3];
		memcpy (hex, &input[i*2], 2);
		hex[2] = '\0';
		output[i] = (unsigned char) strtol (hex, NULL, 16);
	}

	ret = PianoCryptDecrypt (h, output, outputLen);
	if (!ret) {
		free (output);
		return NULL;
	}

	*retSize = outputLen;

	return (char *) output;
}

/*	blowfish-encrypt/hex-encode string
 *	@param cipher handle
 *	@param encrypt this
 *	@return encrypted, hex-encoded string
 */
char *PianoEncryptString (PianoCipher_t h, const char *s) {
	unsigned char *paddedInput, *hexOutput;
	size_t inputLen = strlen (s);
	/* blowfish expects two 32 bit blocks */
	size_t paddedInputLen = (inputLen % 8 == 0) ? inputLen : inputLen + (8-inputLen%8);
	bool ret;

	paddedInput = calloc (paddedInputLen+1, sizeof (*paddedInput));
	memcpy (paddedInput, s, inputLen);

	ret = PianoCryptEncrypt (h, paddedInput, paddedInputLen);
	if (!ret) {
		free (paddedInput);
		return NULL;
	}

	hexOutput = calloc (paddedInputLen*2+1, sizeof (*hexOutput));
	for (size_t i = 0; i < paddedInputLen; i++) {
		snprintf ((char * restrict) &hexOutput[i*2], 3, "%02x", paddedInput[i]);
	}

	free (paddedInput);

	return (char *) hexOutput;
}

