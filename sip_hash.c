/*
wrapper to use SipHash in garnish
Nik Sultana, University of Pennsylvania, March 2018

NOTE this file is based on garnish_hash.c
*/

#include <stddef.h>
#include <stdlib.h>

int halfsiphash(const uint8_t *in, const size_t inlen, const uint8_t *k,
    uint8_t *out, const size_t outlen);

static uint16_t hash_of_uint32_to_uint16(uint32_t data);

GARN(key_t)
GARN(hash_of_KEY_TYPE)(int k, GARN(key_t) data)
{
  //assert(2 = sizeof(GARN(key_t)));
  uint8_t * hash = malloc(sizeof(GARN(key_t)));
  uint8_t hash_key = (uint8_t)k;

  halfsiphash((const uint8_t *)&data, sizeof(GARN(key_t)),
      &hash_key, hash, sizeof(GARN(key_t)));

  GARN(key_t) result = (GARN(key_t))(*hash);
  free(hash);

  return ((GARN(key_t))result) % TABLE_SIZE;
}

// NOTE copied from garnish_hash.c
static uint16_t
hash_of_uint32_to_uint16(uint32_t data)
{
  union {
    uint32_t as_uint32_t;
    uint16_t as_uint16_t[2];
  } conversion;
  conversion.as_uint32_t = data;
  return GARN(hash_of_KEY_TYPE)(0, conversion.as_uint16_t[0]) ^
    GARN(hash_of_KEY_TYPE)(1, conversion.as_uint16_t[1]);
}

// NOTE copied from garnish_hash.c
GARN(key_t)
GARN(fingerprint_of_DATA_TYPE)(GARN(data_t) data)
{
  return hash_of_uint32_to_uint16(data);
}
