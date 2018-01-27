/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017

NOTE: this code is thread-safe in "regular mode", but the debug
      mode (-DREMEMBER_LOSS -DREMEMBER_COLLISIONS) hasn't been tuned for
      thread-safety.
*/


#ifdef PCHAST_ASSERT
#include <assert.h>
#endif // PCHAST_ASSERT
#include <stdlib.h>

#include "pchast.h"

static uint16_t hash_of_uint32_to_uint16(uint32_t data);
static KEY_TYPE hash_of_KEY_TYPE(int k, KEY_TYPE data);

struct entry {
  bool clear;
  KEY_TYPE key;
  VALUE_TYPE value;
};

struct cell {
  struct entry entry[NUM_CELL_ENTRIES];
};

struct table {
  struct cell cell[TABLE_SIZE];
#ifdef MULTITHREADED
  pthread_mutex_t lock[TABLE_SIZE];
#endif // MULTITHREADED
};

#ifdef LOG_INSERTS
#include <stdio.h>
#endif // LOG_INSERTS

#ifdef REMEMBER_LOSS
#include <assert.h>
#include "stdio.h"

struct overfill_t {
  struct entry entry[NUM_OVERFILL_ENTRIES];
} overfill;

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

bool
has_overflowed(DATA_TYPE data) {
  // Check if the item is in the "overflow" array.
  bool item_found = false;
  for (int idx = 0; idx < overfill_idx; idx++) {
    if (data == overfill.entry[idx].key) {
      item_found = true;
      break;
    }
  }
  return item_found;
}
#endif // REMEMBER_LOSS

#ifdef REMEMBER_COLLISIONS
#include <assert.h>
#include "stdio.h"

struct collision_t {
  struct entry entry[NUM_COLLIDED_ENTRIES];
  struct entry collided_with[NUM_COLLIDED_ENTRIES];
} collision;
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

bool
has_collided(DATA_TYPE data, VALUE_TYPE queried_metadata) {
  assert(collision_idx > 0);
  KEY_TYPE fingerprint = fingerprint_of_DATA_TYPE(data);
  for (int idx = 0; idx < collision_idx; idx++) {
    if (fingerprint == collision.entry[idx].key ||
        fingerprint == collision.collided_with[idx].key) {
      /* FIXME This assertion might be too strong if there's been several
               collisions, which can happen if there's a big disparity
               between the original domain and the table size.
      */
      assert(queried_metadata == collision.entry[idx].value ||
          queried_metadata == collision.collided_with[idx].value);
      return true;
    }
  }
  return false;
}
#endif // REMEMBER_COLLISIONS

const char * outcome_str[] =
  {"OK", "NOT_FOUND", "GAVE_UP", "BLOCKS_FULL"};

KEY_TYPE
hash_of_KEY_TYPE(int k, KEY_TYPE data)
{
  KEY_TYPE hash = data;

  // NOTE based on http://www.azillionmonkeys.com/qed/hash.html
  hash ^= hash << (3 + k);
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> (17 - k);
  hash += hash >> 6;

  return hash % TABLE_SIZE;
}

uint16_t
hash_of_uint32_to_uint16(uint32_t data)
{
  union {
    uint32_t as_uint32_t;
    uint16_t as_uint16_t[2];
  } conversion;
  conversion.as_uint32_t = data;
  return hash_of_KEY_TYPE(0, conversion.as_uint16_t[0]) ^
    hash_of_KEY_TYPE(1, conversion.as_uint16_t[1]);
}

KEY_TYPE
fingerprint_of_DATA_TYPE(DATA_TYPE data)
{
  return hash_of_uint32_to_uint16(1 - data);
}

KEY_TYPE
alt_idx(KEY_TYPE idx, KEY_TYPE fingerprint)
{
  // FIXME assuming CHOICE==2
  KEY_TYPE h1 = hash_of_KEY_TYPE(0, fingerprint);
  KEY_TYPE h2 = hash_of_KEY_TYPE(1, fingerprint);
  if (idx == h1) {
    return h2;
  } else {
    assert(idx == h2);
    return h1;
  }
}

struct idxs
idxs_of_DATA_TYPE(DATA_TYPE data, KEY_TYPE * fingerprint)
{
  struct idxs result;
  *fingerprint = fingerprint_of_DATA_TYPE(data);
  for (int i = 0; i < CHOICES; i++) {
    result.idx[i] = hash_of_KEY_TYPE(i, *fingerprint);
  }
#ifdef PCHAST_ASSERT
  for (int i = 0; i < CHOICES; i++) {
    assert((int)result.idx[i] >= 0);
    assert((int)result.idx[i] < TABLE_SIZE);
  }
#endif // PCHAST_ASSERT
  return result;
}

enum outcome
insert(struct table * t, DATA_TYPE data, DATA_TYPE metadata)
{
  KEY_TYPE fingerprint;
  struct idxs is = idxs_of_DATA_TYPE(data, &fingerprint);
#ifdef LOG_INSERTS
  printf("data=%u metadata=%d fingerprint=%d is.idx[0]=%d is.idx[1]=%d\n",
      data, metadata, fingerprint, is.idx[0], is.idx[1]);
#endif // LOG_INSERTS
  for (int idx = 0; idx < CHOICES; idx++) {
    int table_idx = (int)is.idx[idx];
#ifdef MULTITHREADED
    int error = pthread_mutex_lock(&(t->lock[table_idx]));
    assert(!error);
#endif // MULTITHREADED
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      if (t->cell[table_idx].entry[i].clear) {
        t->cell[table_idx].entry[i].clear = false;
        t->cell[table_idx].entry[i].key = fingerprint;
        t->cell[table_idx].entry[i].value = metadata;
#ifdef MULTITHREADED
        error = pthread_mutex_unlock(&(t->lock[table_idx]));
        assert(!error);
#endif // MULTITHREADED
        return OK;
      }
#ifdef REMEMBER_COLLISIONS
      else {
        assert(collision_idx < NUM_COLLIDED_ENTRIES);
        // FIXME check for collision among the other entries (which might
        //       have been moved to an alternative bucket).
        if (t->cell[table_idx].entry[i].key == fingerprint) {
          collision.entry[collision_idx].key = fingerprint;
          collision.entry[collision_idx].value = metadata;
          collision.collided_with[collision_idx].key = t->cell[table_idx].entry[i].key;
          collision.collided_with[collision_idx].value = t->cell[table_idx].entry[i].value;
#ifdef DESCRIBE_COLLISIONS
          printf("(%d, %d) collided with (%d, %d) on table_idx=%d\n",
          collision.entry[collision_idx].key,
          collision.entry[collision_idx].value,
          collision.collided_with[collision_idx].key,
          collision.collided_with[collision_idx].value,
          table_idx);
#endif // DESCRIBE_COLLISIONS
          collision_idx += 1;
        }
      }
#endif // REMEMBER_COLLISIONS
    }
#ifdef MULTITHREADED
    error = pthread_mutex_unlock(&(t->lock[table_idx]));
    assert(!error);
#endif // MULTITHREADED
  }
#ifdef FAIL_EAGERLY
  return BLOCKS_FULL;
#else
  int table_idx = (int)is.idx[(int)rand() % CHOICES];

  KEY_TYPE swapped_key;
  VALUE_TYPE swapped_value;
  for (int try_num = 0; try_num < MAX_KICKOUTS; try_num++) {
    int entry = (int)rand() % NUM_CELL_ENTRIES;
    // FIXME could iterate through entries to find a free one, rather do
    //       unnecessary kicking by picking a random value for "entry".

    // FIXME check for collision among the other entries (which might
    //       have been moved to an alternative bucket). But beware of
    //       over-reporting collisions, e.g., if 2 entries get kicked
    //       down a similar path, it might be counted as multiple collisions
    //       rather than a single one.
#ifdef MULTITHREADED
    int error = pthread_mutex_lock(&(t->lock[table_idx]));
    assert(!error);
#endif // MULTITHREADED
    swapped_key = t->cell[table_idx].entry[entry].key;
    swapped_value = t->cell[table_idx].entry[entry].value;
    t->cell[table_idx].entry[entry].key = fingerprint;
    t->cell[table_idx].entry[entry].value = metadata;

    if (t->cell[table_idx].entry[entry].clear) {
      t->cell[table_idx].entry[entry].clear = false;
#ifdef MULTITHREADED
      error = pthread_mutex_unlock(&(t->lock[table_idx]));
      assert(!error);
#endif // MULTITHREADED
      // We have filled an empty entry (i.e., it's "clear" flag was set) so
      // there's no need to do further kicking.
      return OK;
    }

    fingerprint = swapped_key;
    metadata = swapped_value;
#ifdef MULTITHREADED
    error = pthread_mutex_unlock(&(t->lock[table_idx]));
    assert(!error);
#endif // MULTITHREADED
   // NOTE in addition to exploring the alternative block we could also explore
   //      a fingerprint's "non-alternative" block for available entries --
   //      that is, pick some other fingerprint in the current block and
   //      attempt to kick it, rather than the current fingerprint; but it's
   //      not obvious which to pick, so the current approach feels simplest.
    table_idx = (int)alt_idx((KEY_TYPE)table_idx, fingerprint);
  }
#ifdef REMEMBER_LOSS
  assert(overfill_idx < NUM_OVERFILL_ENTRIES);
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
#endif // FAIL_EAGERLY
}

enum outcome
delete(struct table * t, DATA_TYPE data)
{
  KEY_TYPE fingerprint;
  struct idxs is = idxs_of_DATA_TYPE(data, &fingerprint);
  for (int idx = 0; idx < CHOICES; idx++) {
    int table_idx = (int)is.idx[idx];
#ifdef MULTITHREADED
    int error = pthread_mutex_lock(&(t->lock[table_idx]));
    assert(!error);
#endif // MULTITHREADED
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      if (! t->cell[table_idx].entry[i].clear &&
          t->cell[table_idx].entry[i].key == fingerprint) {
        t->cell[table_idx].entry[i].clear = true;
#ifdef MULTITHREADED
        error = pthread_mutex_unlock(&(t->lock[table_idx]));
        assert(!error);
#endif // MULTITHREADED
        return OK;
      }
    }
#ifdef MULTITHREADED
    error = pthread_mutex_unlock(&(t->lock[table_idx]));
    assert(!error);
#endif // MULTITHREADED
  }
  return NOT_FOUND;
}

enum outcome
lookup(struct table * t, DATA_TYPE data, DATA_TYPE * metadata)
{
  KEY_TYPE fingerprint;
  struct idxs is = idxs_of_DATA_TYPE(data, &fingerprint);
  for (int idx = 0; idx < CHOICES; idx++) {
    int table_idx = (int)is.idx[idx];
#ifdef MULTITHREADED
    int error = pthread_mutex_lock(&(t->lock[table_idx]));
    assert(!error);
#endif // MULTITHREADED
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      if (! t->cell[table_idx].entry[i].clear &&
          t->cell[table_idx].entry[i].key == fingerprint) {
        *metadata = t->cell[table_idx].entry[i].value;
#ifdef MULTITHREADED
        error = pthread_mutex_unlock(&(t->lock[table_idx]));
        assert(!error);
#endif // MULTITHREADED
        return OK;
      }
    }
#ifdef MULTITHREADED
    error = pthread_mutex_unlock(&(t->lock[table_idx]));
    assert(!error);
#endif // MULTITHREADED
  }
  return NOT_FOUND;
}

struct table *
create_table(void)
{
  struct table * t = malloc(sizeof(*t));
  for (int table_idx = 0; table_idx < TABLE_SIZE; table_idx++) {
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      t->cell[table_idx].entry[i].clear = true;
    }
#ifdef MULTITHREADED
    int error = pthread_mutex_init(&(t->lock[table_idx]), NULL);
    assert(!error);
#endif // MULTITHREADED
  }
  return t;
}

void
destroy_table(struct table * t)
{
  for (int table_idx = 0; table_idx < TABLE_SIZE; table_idx++) {
#ifdef MULTITHREADED
    int error = pthread_mutex_destroy(&(t->lock[table_idx]));
    assert(!error);
#endif // MULTITHREADED
  }
  free(t);
}

int
rand_range(int min, int max)
{
  assert(min >= 0);
  assert(max >= min);

  if (min == max) {
    return min;
  }

  return min + (rand() % (max - min));
}
