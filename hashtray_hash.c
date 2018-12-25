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


This file: hashing functions for use in hashtray
*/

static uint16_t hash_of_uint32_to_uint16(uint32_t data);

HASHTRAY(key_t)
HASHTRAY(hash_of_KEY_TYPE)(int k, HASHTRAY(key_t) data)
{
/* FIXME old
  int hash = data + (data * k) + k;
  // NOTE based on http://www.azillionmonkeys.com/qed/hash.html
  hash ^= hash << (3 + k);
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> (17 - k);
  hash += hash << 6;
*/

  // NOTE based on djb2 at: http://www.cse.yorku.ca/~oz/hash.html
  long long hash = 5381 * k + k;
  char * buf = (char *)&data;
  for (unsigned i = 0; i < sizeof(HASHTRAY(data_t)); i++) {
    hash = hash * 33 ^ buf[i];
  }

  return ((HASHTRAY(key_t))hash) % TABLE_SIZE;
}

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

HASHTRAY(key_t)
HASHTRAY(fingerprint_of_DATA_TYPE)(HASHTRAY(data_t) data)
{
  return hash_of_uint32_to_uint16(data);
}
