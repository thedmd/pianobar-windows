/*
Copyright (c) 2008-2012
	Lars-Dominik Braun <lars@6xq.net>
Copyright (c) 2012
    Micha3 Cichon <michcic@gmail.com>

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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "crypt.h"

#ifdef _MSC_VER
#define crypt_snprintf                  _snprintf
#else
#define crypt_snprintf                  snprintf
#endif

/*	decrypt hex-encoded, blowfish-crypted string: decode 2 hex-encoded blocks,
 *	decrypt, byteswap
 *	@param BLOWFISH_CTX handle
 *	@param hex string
 *	@param decrypted string length (without trailing NUL)
 *	@return decrypted string or NULL
 */
char *PianoDecryptString (BLOWFISH_CTX * ctx, const char * const input,
		size_t * const retSize) {
    size_t inputLen = strlen (input);
    int ret;
    unsigned char *output;
    size_t outputLen = inputLen/2;
    size_t i;

    assert (inputLen%2 == 0);

    output = calloc (outputLen+1, sizeof (*output));
    /* hex decode */
    for (i = 0; i < outputLen; i++) {
        char hex[3];
        memcpy (hex, &input[i*2], 2);
        hex[2] = '\0';
        output[i] = (char)strtol (hex, NULL, 16);
    }

    ret = Blowfish_DecryptData (ctx, (uint32_t*)output, (uint32_t*)output, outputLen);
    if (BLOWFISH_OK != ret) {
        fprintf (stderr, "Failure: Data block is not aligned to 64-bytes/\n");
        return NULL;
    }

    *retSize = outputLen;

    return (char *) output;
}

/*	blowfish-encrypt/hex-encode string
 *	@param BLOWFISH_CTX handle
 *	@param encrypt this
 *	@return encrypted, hex-encoded string
 */
char *PianoEncryptString (BLOWFISH_CTX * ctx, const char *s) {
    unsigned char *paddedInput, *hexOutput;
    size_t inputLen = strlen (s);
    /* blowfish expects two 32 bit blocks */
    size_t paddedInputLen = (inputLen % 8 == 0) ? inputLen : inputLen + (8-inputLen%8);
    size_t i;
    int ret;

    paddedInput = calloc (paddedInputLen+1, sizeof (*paddedInput));
    memcpy (paddedInput, s, inputLen);

    ret = Blowfish_EncryptData (ctx, (uint32_t*)paddedInput, (uint32_t*)paddedInput, paddedInputLen);
    if (BLOWFISH_OK != ret) {
        fprintf (stderr, "Failure: Data block is not aligned to 64-bytes/\n");
        return NULL;
    }

    hexOutput = calloc (paddedInputLen*2+1, sizeof (*hexOutput));
    for (i = 0; i < paddedInputLen; i++) {
        crypt_snprintf (&hexOutput[i*2], 3, "%02x", paddedInput[i]);
    }

    free (paddedInput);

    return (char *) hexOutput;
}
