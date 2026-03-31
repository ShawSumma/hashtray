// Wrapper to use SipHash in hashtray, based on hashtray_hash.c.

#include <stddef.h>
#include <string.h>

#include "common.h"

int halfsiphash(
  const uint8_t *in,
  const size_t inlen,
  const uint8_t *k,
  uint8_t *out,
  const size_t outlen
);

hashtray_key_t hashtray_hash_key(int32_t k, hashtray_key_t data) {
  uint8_t hash[sizeof(hashtray_key_t)];
  uint8_t hash_k = (uint8_t) k;

  halfsiphash(
    (const uint8_t *) &data,
    sizeof(hashtray_key_t),
    &hash_k,
    hash,
    sizeof(hashtray_key_t)
  );

  hashtray_key_t result = (hashtray_key_t) hash[0];

  return ((hashtray_key_t) result) % TABLE_SIZE;
}

static uint16_t hash_uint32_to_uint16(uint32_t data) {
  uint16_t halves[2];
  memcpy(halves, &data, sizeof(data));
  return hashtray_hash_key(0, halves[0])
    ^ hashtray_hash_key(1, halves[1]);
}

hashtray_key_t hashtray_fingerprint(hashtray_data_t data) {
  return hash_uint32_to_uint16(data);
}
