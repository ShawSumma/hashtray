/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017
*/


#ifdef PCHAST_ASSERT
#include <assert.h>
#endif // PCHAST_ASSERT
#include <stdlib.h>

#include "pchast.h"

#ifdef LOG_INSERTS
#include <stdio.h>
#endif // LOG_INSERTS

#ifdef REMEMBER_LOSS
#include <assert.h>
#include "stdio.h"

struct overfill_t overfill;
int overfill_idx = 0;

void
print_overfill(bool show_entries)
{
  assert(overfill_idx >= 0);
  printf("overfill_idx=%d\n", overfill_idx);
  if (show_entries) {
    for (int idx = 0; idx < overfill_idx; idx++) {
      printf("%d. key=%d, value=%d\n", idx, overfill.entry[idx].key,
          overfill.entry[idx].value);
    }
  }
}

void
reset_overfill(void)
{
  overfill_idx = 0;
}
#endif // REMEMBER_LOSS

#ifdef REMEMBER_COLLISIONS
#include <assert.h>
#include "stdio.h"

struct collision_t collision;
int collision_idx = 0;

void
print_collision(bool show_entries)
{
  assert(collision_idx >= 0);
  printf("collision_idx=%d\n", collision_idx);
  if (show_entries) {
    for (int idx = 0; idx < collision_idx; idx++) {
      printf("%d. key=%d, value=%d (collided with key=%d, value=%d)\n",
          idx, collision.entry[idx].key, collision.entry[idx].value,
          collision.collided_with[idx].key, collision.collided_with[idx].value);
    }
  }
}

void
reset_collision(void)
{
  collision_idx = 0;
}
#endif // REMEMBER_COLLISIONS

const char * outcome_str[] =
  {"OK", "NOT_FOUND", "GAVE_UP", "BLOCKS_FULL"};

static union {
  uint64_t u64;
  uint32_t u32[2];
} prng_state;

void
init_prng(uint64_t seed)
{
  prng_state.u64 = seed;
}

uint32_t
prng(void)
{
  // Based on the middle-square method.
  prng_state.u64 *= 2 + prng_state.u64;
  uint32_t temp = prng_state.u32[0];
  prng_state.u32[0] = prng_state.u32[1] >> 16;
  prng_state.u32[1] = temp << 16;
  return (uint32_t)prng_state.u64;
}

uint8_t
hash_of_uint32_to_uint8(uint32_t data)
{
  union {
    uint32_t as_uint32_t;
    uint8_t as_byte_array[4];
  } conversion;
  conversion.as_uint32_t = data;
  return conversion.as_byte_array[0] ^
    conversion.as_byte_array[1] ^
    conversion.as_byte_array[2] ^
    conversion.as_byte_array[3];
}

uint8_t
hash_of_uint8_to_uint8(uint8_t data)
{
  return data;
}

KEY_TYPE
hash_of_KEY_TYPE(KEY_TYPE data)
{
  return hash_of_uint8_to_uint8(data);
}

KEY_TYPE
hash_of_DATA_TYPE(DATA_TYPE data)
{
  return hash_of_uint32_to_uint8(data);
}

KEY_TYPE
fingerprint_of_DATA_TYPE(DATA_TYPE data)
{
  return hash_of_uint32_to_uint8(1 - data);
}

KEY_TYPE
alt_idx(KEY_TYPE idx, KEY_TYPE fingerprint)
{
  return idx ^ hash_of_KEY_TYPE(fingerprint);
}

struct idxs
idxs_of_DATA_TYPE(DATA_TYPE data, KEY_TYPE * fingerprint)
{
  struct idxs result;
  *fingerprint = fingerprint_of_DATA_TYPE(data);
  // NOTE here we assume that CHOICES==2
  result.idx[0] = hash_of_DATA_TYPE(data);
  result.idx[1] = result.idx[0] ^ hash_of_KEY_TYPE(*fingerprint);
#ifdef PCHAST_ASSERT
  assert((int)result.idx[0] >= 0);
  assert((int)result.idx[1] >= 0);
#endif // PCHAST_ASSERT
  return result;
}

enum outcome
insert(table * t, DATA_TYPE data, DATA_TYPE metadata)
{
  KEY_TYPE fingerprint;
  struct idxs is = idxs_of_DATA_TYPE(data, &fingerprint);
#ifdef LOG_INSERTS
  printf("%u %d %d %d %d\n", data, metadata, fingerprint,
      is.idx[0], is.idx[1]);
#endif // LOG_INSERTS
  for (int idx = 0; idx < CHOICES; idx++) {
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      int table_idx = (int)is.idx[idx];
      if ((*t)[table_idx].entry[i].clear) {
        (*t)[table_idx].entry[i].clear = false;
        (*t)[table_idx].entry[i].key = fingerprint;
        (*t)[table_idx].entry[i].value = metadata;
        return OK;
      }
#ifdef REMEMBER_COLLISIONS
      else {
        // FIXME check for collision among the other entries (which might
        //       have been moved to an alternative bucket).
        if ((*t)[table_idx].entry[i].key == fingerprint) {
          collision.entry[collision_idx].key = fingerprint;
          collision.entry[collision_idx].value = metadata;
          collision.collided_with[collision_idx].key = (*t)[table_idx].entry[i].key;
          collision.collided_with[collision_idx].value = (*t)[table_idx].entry[i].value;
#ifdef DESCRIBE_COLLISIONS
          printf("(%d, %d) collided with (%d, %d)\n",
          collision.entry[collision_idx].key,
          collision.entry[collision_idx].value,
          collision.collided_with[collision_idx].key,
          collision.collided_with[collision_idx].value);
#endif
          collision_idx += 1;
        }
      }
#endif // REMEMBER_COLLISIONS
    }
  }
#ifdef FAIL_EAGERLY
  return BLOCKS_FULL;
#else
  KEY_TYPE table_idx;
#ifdef LAME_KICK_SEQUENCE
  #define DEFAULT_IDX 0
  table_idx = is.idx[DEFAULT_IDX];
#else
  table_idx = is.idx[(int)prng() % CHOICES];
#endif

  int entry;
#ifdef LAME_KICK_SEQUENCE
    #define DEFAULT_ENTRY 0
    entry = DEFAULT_ENTRY;
#endif

  KEY_TYPE swapped_key;
  VALUE_TYPE swapped_value;
  for (int try = 0; try < MAX_KICKOUTS; try++) {
#ifndef LAME_KICK_SEQUENCE
    entry = (int)prng() % NUM_CELL_ENTRIES;
#endif

    // FIXME check for collision among the other entries (which might
    //       have been moved to an alternative bucket).
    swapped_key = (*t)[(int)table_idx].entry[entry].key;
    swapped_value = (*t)[(int)table_idx].entry[entry].value;
    (*t)[(int)table_idx].entry[entry].key = fingerprint;
    (*t)[(int)table_idx].entry[entry].value = metadata;

    if ((*t)[(int)table_idx].entry[entry].clear) {
      (*t)[(int)table_idx].entry[entry].clear = false;
      return OK;
    }

    fingerprint = swapped_key;
    metadata = swapped_value;
   // NOTE in addition to exploring the alternative block we could also explore
   //      a fingerprint's "non-alternative" block for available entries --
   //      that is, pick some other fingerprint in the current block and
   //      attempt to kick it, rather than the current fingerprint; but it's
   //      not obvious which to pick, so the current approach feels simplest.
    table_idx = alt_idx(table_idx, fingerprint);
  }
#ifdef REMEMBER_LOSS
  // Record which items got kicked out of the table.
  // NOTE This behaviour might be exploited, to have elements of the table
  //      erased (having them kicked out), if an adversary can engineer a
  //      series of moves.
  overfill.entry[overfill_idx].clear = false;
  overfill.entry[overfill_idx].key = fingerprint;
  overfill.entry[overfill_idx].value = metadata;
  overfill_idx += 1;
#endif // REMEMBER_LOSS
  return GAVE_UP;
#endif
}

enum outcome
delete(table * t, DATA_TYPE data)
{
  KEY_TYPE fingerprint;
  struct idxs is = idxs_of_DATA_TYPE(data, &fingerprint);
  for (int idx = 0; idx < CHOICES; idx++) {
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      int table_idx = (int)is.idx[idx];
      if (!(*t)[table_idx].entry[i].clear &&
          (*t)[table_idx].entry[i].key == fingerprint) {
        (*t)[table_idx].entry[i].clear = true;
        return OK;
      }
    }
  }
  return NOT_FOUND;
}

enum outcome
lookup(table * t, DATA_TYPE data, DATA_TYPE * metadata)
{
  KEY_TYPE fingerprint;
  struct idxs is = idxs_of_DATA_TYPE(data, &fingerprint);
  for (int idx = 0; idx < CHOICES; idx++) {
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      KEY_TYPE table_idx = is.idx[idx];
      if (!(*t)[(int)table_idx].entry[i].clear &&
          (*t)[(int)table_idx].entry[i].key == fingerprint) {
        *metadata = (*t)[(int)table_idx].entry[i].value;
        return OK;
      }
    }
  }
  return NOT_FOUND;
}

table *
create_table(void)
{
  table * t = malloc(sizeof(*t));
  for (int idx = 0; idx < TABLE_SIZE; idx++) {
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      (*t)[idx].entry[i].clear = true;
    }
  }
  return t;
}

void
destroy_table(table * t)
{
  free(t);
}
