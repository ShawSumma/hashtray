#include <string.h>

#include "common.h"

hashtray_key_t hashtray_hash_key(int32_t k, hashtray_key_t data) {
  int64_t hash = 5381 * k + k;
  char *buf = (char *) &data;
  for (uint32_t i = 0; i < sizeof(hashtray_key_t); i++) {
    hash = hash * 33 ^ buf[i];
  }
  return ((hashtray_key_t) hash) % TABLE_SIZE;
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
