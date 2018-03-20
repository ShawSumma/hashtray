/*
hashing functions for use in garnish
Nik Sultana, University of Pennsylvania, November 2017
*/

static uint16_t hash_of_uint32_to_uint16(uint32_t data);

GARN(key_t)
GARN(hash_of_KEY_TYPE)(int k, GARN(key_t) data)
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
  for (unsigned i = 0; i < sizeof(GARN(data_t)); i++) {
    hash = hash * 33 ^ buf[i];
  }

  return ((GARN(key_t))hash) % TABLE_SIZE;
}

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

GARN(key_t)
GARN(fingerprint_of_DATA_TYPE)(GARN(data_t) data)
{
  return hash_of_uint32_to_uint16(data);
}
