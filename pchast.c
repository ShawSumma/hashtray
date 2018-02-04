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

#ifdef REMEMBER_COLLISIONS
#include <assert.h>
#include "stdio.h"
#endif // REMEMBER_COLLISIONS

#ifdef LOG_INSERTS
#include <stdio.h>
#endif // LOG_INSERTS

#ifdef REMEMBER_LOSS
#include <assert.h>
#include "stdio.h"
#endif // REMEMBER_LOSS

#if defined(REMEMBER_LOSS) || defined(REMEMBER_COLLISIONS)
#include "pchast_debug.h"
#endif // defined(REMEMBER_LOSS) || defined(REMEMBER_COLLISIONS)

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#if defined(MULTITHREADED) && defined(MULTIPROCESS)
#error Simultaneous MULTITHREADED and MULTIPROCESS not supported.
#endif // defined(MULTITHREADED) && defined(MULTIPROCESS)

#ifdef MULTITHREADED
#include <pthread.h>
#endif // MULTITHREADED

#ifdef MULTIPROCESS
#include <stdio.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/types.h>
#endif // MULTIPROCESS

struct idxs {
  PCH(key_t) idx[CHOICES];
};

static PCH(key_t) alt_idx(PCH(key_t) idx, PCH(key_t) fingerprint);
static struct idxs idxs_of_DATA_TYPE(PCH(data_t) data, PCH(key_t) * fingerprint);

static PCH(key_t) fingerprint_of_DATA_TYPE(PCH(data_t) data);

// FIXME ideally hashing functions would be parameters
static uint16_t hash_of_uint32_to_uint16(uint32_t data);
static PCH(key_t) hash_of_KEY_TYPE(int k, PCH(key_t) data);

struct entry {
  bool clear;
  PCH(key_t) key;
  PCH(value_t) value;
};

struct cell {
  struct entry entry[NUM_CELL_ENTRIES];
};

struct PCH(table) {
  struct cell cell[TABLE_SIZE];
#ifdef MULTITHREADED
  pthread_mutex_t lock[TABLE_SIZE];
#endif // MULTITHREADED
#ifdef MULTIPROCESS
  sem_t * lock[TABLE_SIZE];
#endif // MULTIPROCESS
};

#ifdef REMEMBER_LOSS
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
has_overflowed(PCH(data_t) data) {
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
has_collided(PCH(data_t) data, PCH(value_t) queried_metadata) {
  assert(collision_idx > 0);
  PCH(key_t) fingerprint = fingerprint_of_DATA_TYPE(data);
  bool found_collision = false;
  for (int idx = 0; idx < collision_idx; idx++) {
    if (fingerprint == collision.entry[idx].key ||
        fingerprint == collision.collided_with[idx].key) {
      if (queried_metadata == collision.entry[idx].value ||
          queried_metadata == collision.collided_with[idx].value) {
        found_collision = true;
        break;
      }
    }
  }
  return found_collision;
}
#endif // REMEMBER_COLLISIONS

const char * PCH(outcome_str)[] =
  {"OK", "NOT_FOUND", "GAVE_UP", "BLOCKS_FULL"};

static PCH(key_t)
hash_of_KEY_TYPE(int k, PCH(key_t) data)
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
  for (unsigned i = 0; i < sizeof(PCH(data_t)); i++) {
    hash = hash * 33 ^ buf[i];
  }

  return ((PCH(key_t))hash) % TABLE_SIZE;
}

static uint16_t
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

static PCH(key_t)
fingerprint_of_DATA_TYPE(PCH(data_t) data)
{
  return hash_of_uint32_to_uint16(data);
}

static PCH(key_t)
alt_idx(PCH(key_t) idx, PCH(key_t) fingerprint)
{
  // FIXME assuming CHOICE==2
  PCH(key_t) h1 = hash_of_KEY_TYPE(0, fingerprint);
  PCH(key_t) h2 = hash_of_KEY_TYPE(1, fingerprint);
  if (idx == h1) {
    return h2;
  } else {
#ifdef PCHAST_ASSERT
    assert(idx == h2);
#endif // PCHAST_ASSERT
    return h1;
  }
}

struct idxs
idxs_of_DATA_TYPE(PCH(data_t) data, PCH(key_t) * fingerprint)
{
  struct idxs result;
  *fingerprint = fingerprint_of_DATA_TYPE(data);
  for (int i = 0; i < CHOICES; i++) {
    result.idx[i] = hash_of_KEY_TYPE(i, *fingerprint);
  }
  // FIXME how do we ensure that, for all i and j, result.idx[i] != result.idx[j]
#ifdef PCHAST_ASSERT
  for (int i = 0; i < CHOICES; i++) {
    assert((int)result.idx[i] >= 0);
    assert((int)result.idx[i] < TABLE_SIZE);
  }
#endif // PCHAST_ASSERT
  return result;
}

static inline void
lock_indices(struct PCH(table) * t, struct idxs is) {
#if !defined(MULTITHREADED) && !defined(MULTIPROCESS)
  // Do nothing

#elif defined(MULTITHREADED) && defined(MULTIPROCESS)
#error Simultaneous MULTITHREADED and MULTIPROCESS not supported.

#else
  // We lock both choices at a go, but owing to the sequential nature of
  // locking we could end up with deadlock, so we give up after a short timeout
  // and retry, to give the other side the chance to lock.
  while (true) { // FIXME risk of infinite execution
    int idx = 0;
    for (; idx < CHOICES; idx++) {
      int result;

#ifdef MULTITHREADED
      result = pthread_mutex_trylock(&(t->lock[(int)is.idx[idx]])); // FIXME strictly speaking should check for EBUSY == result
#endif // MULTITHREADED

#ifdef MULTIPROCESS
      result = sem_trywait(t->lock[(int)is.idx[idx]]);
#endif // MULTIPROCESS

      if (0 != result) {
        // Unlock everything, wait a tiny amount of time and try again.
        for (int idy = 0; idy < idx; idy++) {
#ifdef MULTITHREADED
          int error = pthread_mutex_unlock(&(t->lock[(int)is.idx[idy]]));
#ifdef PCHAST_ASSERT
          assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT
#endif // MULTITHREADED

#ifdef MULTIPROCESS
          sem_post(t->lock[(int)is.idx[idy]]); // FIXME check return value
#endif // MULTIPROCESS
        }
        struct timespec req = {.tv_sec = 0,
          .tv_nsec = (1000 * (rand() % BACKOFF_SLEEP_MICROSEC))};
        struct timespec rem;
        nanosleep(&req, &rem); // FIXME ignoring return value
        break;
      }
    }

    if (CHOICES == idx) {
      break;
    }
  }
#endif
}

static inline void
unlock_indices_except(struct PCH(table) * t, struct idxs is, int * opt_dont_unlock) {
#if !defined(MULTITHREADED) && !defined(MULTIPROCESS)
  // Do nothing

#elif defined(MULTITHREADED) && defined(MULTIPROCESS)
#error Simultaneous MULTITHREADED and MULTIPROCESS not supported.

#else
  for (int idx = 0; idx < CHOICES; idx++) {
    int table_idx = (int)is.idx[idx];
    if (NULL == opt_dont_unlock ||
        (NULL != opt_dont_unlock &&
         *opt_dont_unlock != table_idx)) {
      int error;
#ifdef MULTITHREADED
      error = pthread_mutex_unlock(&(t->lock[table_idx]));
#ifdef PCHAST_ASSERT
      assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT
#endif // MULTITHREADED

#ifdef MULTIPROCESS
      sem_post(t->lock[table_idx]); // FIXME check return value
#endif // MULTIPROCESS
    }
  }

#endif
}

static inline void
unlock_index(struct PCH(table) * t, int table_idx) {
#if !defined(MULTITHREADED) && !defined(MULTIPROCESS)
  // Do nothing

#elif defined(MULTITHREADED) && defined(MULTIPROCESS)
#error Simultaneous MULTITHREADED and MULTIPROCESS not supported.

#elif defined(MULTITHREADED)
  int error = pthread_mutex_unlock(&(t->lock[table_idx]));
#ifdef PCHAST_ASSERT
  assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT

#elif defined(MULTITHREADED)
  sem_post(t->lock[table_idx]); // FIXME check return value
#endif
}

static inline void
lock_index(struct PCH(table) * t, int table_idx) {
#if !defined(MULTITHREADED) && !defined(MULTIPROCESS)
  // Do nothing

#elif defined(MULTITHREADED) && defined(MULTIPROCESS)
#error Simultaneous MULTITHREADED and MULTIPROCESS not supported.

#elif defined(MULTITHREADED)
  int error = pthread_mutex_lock(&(t->lock[table_idx]));
#ifdef PCHAST_ASSERT
  assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT

#elif defined(MULTITHREADED)
  sem_wait(t->lock[table_idx]); // FIXME check return value
#endif
}

enum PCH(outcome)
PCH(insert)(struct PCH(table) * t, PCH(data_t) data, PCH(data_t) metadata)
{
  PCH(key_t) fingerprint;
  struct idxs is = idxs_of_DATA_TYPE(data, &fingerprint);
#ifdef LOG_INSERTS
  printf("data=%u metadata=%d fingerprint=%d is.idx[0]=%d is.idx[1]=%d\n",
      data, metadata, fingerprint, is.idx[0], is.idx[1]);
#endif // LOG_INSERTS

  lock_indices(t, is);

#ifdef REMEMBER_COLLISIONS
  for (int idx = 0; idx < CHOICES; idx++) {
    int table_idx = (int)is.idx[idx];
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      if (!t->cell[table_idx].entry[i].clear &&
          t->cell[table_idx].entry[i].key == fingerprint) {
        // We judge that a collision has occurred.
#ifdef PCHAST_ASSERT
        assert(collision_idx < NUM_COLLIDED_ENTRIES);
#endif // PCHAST_ASSERT
        collision.entry[collision_idx].key = fingerprint;
        collision.entry[collision_idx].value = metadata;
        collision.collided_with[collision_idx].key = t->cell[table_idx].entry[i].key;
        collision.collided_with[collision_idx].value = t->cell[table_idx].entry[i].value;
#ifdef DESCRIBE_COLLISIONS
        printf("(%d, %d) collided with (%d, %d) on table_idx=%d, entry=%d\n",
        collision.entry[collision_idx].key,
        collision.entry[collision_idx].value,
        collision.collided_with[collision_idx].key,
        collision.collided_with[collision_idx].value,
        table_idx, i);
#endif // DESCRIBE_COLLISIONS
        collision_idx += 1;
      }
    }
  }
#endif // REMEMBER_COLLISIONS

  // This is the bit that actually does the inserting.
  // We check both alternatives before updating the table otherwise we could
  // end up with multiple values for the same key.
  bool exists = false;
  int table_idx;
  int entry_idx;
  // Keep track of free entries we can insert into.
  bool found_free_entry = false;
  int free_table_idx;
  int free_entry_idx;
  for (int idx = 0; idx < CHOICES; idx++) {
    table_idx = (int)is.idx[idx];
    for (entry_idx = 0; entry_idx < NUM_CELL_ENTRIES; entry_idx++) {
      if (t->cell[table_idx].entry[entry_idx].clear &&
          !found_free_entry/*Only need one free entry*/) {
        found_free_entry = true;
        free_table_idx = table_idx;
        free_entry_idx = entry_idx;
      }

      if (!t->cell[table_idx].entry[entry_idx].clear &&
        (t->cell[table_idx].entry[entry_idx].key == fingerprint)) {
        exists = true;
        break;
      }
    }

    if (exists) {
      break;
    }
  }

  if (exists) {
#ifdef PCHAST_ASSERT
    assert(!t->cell[table_idx].entry[entry_idx].clear);
    assert(t->cell[table_idx].entry[entry_idx].key == fingerprint);
#endif // PCHAST_ASSERT
    t->cell[table_idx].entry[entry_idx].value = metadata;
  } else if (found_free_entry) {
    t->cell[free_table_idx].entry[free_entry_idx].clear = false;
    t->cell[free_table_idx].entry[free_entry_idx].key = fingerprint;
    t->cell[free_table_idx].entry[free_entry_idx].value = metadata;
  }

  if (exists || found_free_entry) {
    // We can unlock everything and return.
    unlock_indices_except(t, is, NULL);
    return PCH(OK);
  }

  // At this point we haven't been able to make the insertion, since both cells
  // were already full.
#ifdef FAIL_EAGERLY
  // Unlock everything and give up.
  unlock_indices_except(t, is, NULL);

  return PCH(BLOCKS_FULL);
#else // ndef FAIL_EAGERLY

  table_idx = (int)is.idx[(int)rand() % CHOICES];

  // We're going to have to kick stuff out. We unlock all except the cell we
  // choose _not_ to kick stuff out of and continue.
  unlock_indices_except(t, is, &table_idx);

  PCH(key_t) swapped_key;
  PCH(value_t) swapped_value;
  for (int try_num = 0; try_num < MAX_KICKOUTS; try_num++) {
    int entry = (int)rand() % NUM_CELL_ENTRIES;
    // FIXME could iterate through entries to find a free one, rather do
    //       unnecessary kicking by picking a random value for "entry".

    // FIXME check for collision among the other entries (which might
    //       have been moved to an alternative bucket). But beware of
    //       over-reporting collisions, e.g., if 2 entries get kicked
    //       down a similar path, it might be counted as multiple collisions
    //       rather than a single one.

    swapped_key = t->cell[table_idx].entry[entry].key;
    swapped_value = t->cell[table_idx].entry[entry].value;
    // t->cell[table_idx] is already locked at this point, so we can update it
    // safely.
    t->cell[table_idx].entry[entry].key = fingerprint;
    t->cell[table_idx].entry[entry].value = metadata;

    // This won't be true the first time we go through this loop, since we
    // wouldn't have entered the "kick out" phase if there were empty cells.
    if (t->cell[table_idx].entry[entry].clear) {
      t->cell[table_idx].entry[entry].clear = false;
      unlock_index(t, table_idx);
      // We have filled an empty entry (i.e., it's "clear" flag was set) so
      // there's no need to do further kicking.
      return PCH(OK);
    }

    fingerprint = swapped_key;
    metadata = swapped_value;
    unlock_index(t, table_idx);
   // NOTE in addition to exploring the alternative block we could also explore
   //      a fingerprint's "non-alternative" block for available entries --
   //      that is, pick some other fingerprint in the current block and
   //      attempt to kick it, rather than the current fingerprint; but it's
   //      not obvious which to pick, so the current approach feels simplest.
    table_idx = (int)alt_idx((PCH(key_t))table_idx, fingerprint);
    lock_index(t, table_idx);
  }

  // If we reached this point then we exceeded MAX_KICKOUTS. We're giving up
  // with the propagation.
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
  return PCH(GAVE_UP);
#endif // FAIL_EAGERLY
}

enum PCH(outcome)
PCH(delete)(struct PCH(table) * t, PCH(data_t) data)
{
  enum PCH(outcome) result = PCH(NOT_FOUND);
  PCH(key_t) fingerprint;
  struct idxs is = idxs_of_DATA_TYPE(data, &fingerprint);
  lock_indices(t, is);

  for (int idx = 0; idx < CHOICES; idx++) {
    int table_idx = (int)is.idx[idx];
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      if (! t->cell[table_idx].entry[i].clear &&
          t->cell[table_idx].entry[i].key == fingerprint) {
        t->cell[table_idx].entry[i].clear = true;
        result = PCH(OK);
        break;
      }
    }

    if (PCH(OK) == result) {
      break;
    }
  }

  unlock_indices_except(t, is, NULL);
  return result;
}

enum PCH(outcome)
PCH(lookup)(struct PCH(table) * t, PCH(data_t) data, PCH(data_t) * metadata)
{
  enum PCH(outcome) result = PCH(NOT_FOUND);
  PCH(key_t) fingerprint;
  struct idxs is = idxs_of_DATA_TYPE(data, &fingerprint);
  lock_indices(t, is);

  for (int idx = 0; idx < CHOICES; idx++) {
    int table_idx = (int)is.idx[idx];
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      if (! t->cell[table_idx].entry[i].clear &&
          t->cell[table_idx].entry[i].key == fingerprint) {
        *metadata = t->cell[table_idx].entry[i].value;
        result = PCH(OK);
        break;
      }
    }

    if (PCH(OK) == result) {
      break;
    }
  }

  unlock_indices_except(t, is, NULL);
  return result;
}

enum PCH(outcome)
PCH(update)(struct PCH(table) * t, PCH(data_t) data, PCH(data_t) metadata)
{
  enum PCH(outcome) result = PCH(NOT_FOUND);
  PCH(key_t) fingerprint;
  struct idxs is = idxs_of_DATA_TYPE(data, &fingerprint);
  lock_indices(t, is);

  for (int idx = 0; idx < CHOICES; idx++) {
    int table_idx = (int)is.idx[idx];
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      if (! t->cell[table_idx].entry[i].clear &&
          t->cell[table_idx].entry[i].key == fingerprint) {
        t->cell[table_idx].entry[i].value = metadata;
        result = PCH(OK);
        break;
      }
    }

    if (PCH(OK) == result) {
      break;
    }
  }

  unlock_indices_except(t, is, NULL);
  return result;
}

struct PCH(table) *
PCH(create_table)(void)
{
#ifdef MULTIPROCESS
  struct PCH(table) * t = mmap(NULL, sizeof(*t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
  // FIXME check outcome of mmap()
#else
  struct PCH(table) * t = malloc(sizeof(*t));
#endif // MULTIPROCESS
  for (int table_idx = 0; table_idx < TABLE_SIZE; table_idx++) {
    for (int i = 0; i < NUM_CELL_ENTRIES; i++) {
      t->cell[table_idx].entry[i].clear = true;
    }
#ifdef MULTITHREADED
    int error = pthread_mutex_init(&(t->lock[table_idx]), NULL);
#ifdef PCHAST_ASSERT
    assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT
#endif // MULTITHREADED

#ifdef MULTIPROCESS
    // Not using sem_init() since it's not supported on MacOS;
    // so instead I'm making do with sem_open().
    char name[12];
    sprintf(name, "/PCH_sem_%d"/*FIXME const -- make this into parameter*/,
        table_idx);
    t->lock[table_idx] = sem_open(name, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
#ifdef PCHAST_ASSERT
    assert(SEM_FAILED != t->lock[table_idx]); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT
#endif // MULTIPROCESS
  }
  return t;
}

void
PCH(destroy_table)(struct PCH(table) * t)
{
  for (int table_idx = 0; table_idx < TABLE_SIZE; table_idx++) {
    int error;
#ifdef MULTITHREADED
    error = pthread_mutex_destroy(&(t->lock[table_idx]));
#ifdef PCHAST_ASSERT
    assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT
#endif // MULTITHREADED

#ifdef MULTIPROCESS
    error = sem_close(t->lock[table_idx]); // FIXME check return value.
#ifdef PCHAST_ASSERT
    assert(-1 != error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT
#endif // MULTIPROCESS
  }
#ifdef MULTIPROCESS
  munmap(t, sizeof(*t)); // FIXME check return value
#else
  free(t);
#endif // MULTIPROCESS
}

int
PCH(rand_range)(int min, int max)
{
#ifdef PCHAST_ASSERT
  assert(min >= 0);
  assert(max >= min);
#endif // PCHAST_ASSERT

  if (min == max) {
    return min;
  }

  return min + (rand() % (max - min));
}
