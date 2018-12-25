/*
Copyright University of Pennsylvania, 2018

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


This file: wrapper to use SipHash in hashtray

NOTE this file is based on hashtray_hash.c
*/

#include <stddef.h>
#include <stdlib.h>

int halfsiphash(const uint8_t *in, const size_t inlen, const uint8_t *k,
    uint8_t *out, const size_t outlen);

static uint16_t hash_of_uint32_to_uint16(uint32_t data);

HASHTRAY(key_t)
HASHTRAY(hash_of_KEY_TYPE)(int k, HASHTRAY(key_t) data)
{
  //assert(2 = sizeof(HASHTRAY(key_t)));
  uint8_t * hash = malloc(sizeof(HASHTRAY(key_t)));
  uint8_t hash_key = (uint8_t)k;

  halfsiphash((const uint8_t *)&data, sizeof(HASHTRAY(key_t)),
      &hash_key, hash, sizeof(HASHTRAY(key_t)));

  HASHTRAY(key_t) result = (HASHTRAY(key_t))(*hash);
  free(hash);

  return ((HASHTRAY(key_t))result) % TABLE_SIZE;
}

// NOTE copied from hashtray_hash.c
static uint16_t
hash_of_uint32_to_uint16(uint32_t data)
{
  union {
    uint32_t as_uint32_t;
    uint16_t as_uint16_t[2];
  } conversion;
  conversion.as_uint32_t = data;
  return HASHTRAY(hash_of_KEY_TYPE)(0, conversion.as_uint16_t[0]) ^
    HASHTRAY(hash_of_KEY_TYPE)(1, conversion.as_uint16_t[1]);
}

// NOTE copied from hashtray_hash.c
HASHTRAY(key_t)
HASHTRAY(fingerprint_of_DATA_TYPE)(HASHTRAY(data_t) data)
{
  return hash_of_uint32_to_uint16(data);
}
