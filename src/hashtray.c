// Partial-key cuckoo hash.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hashtray.h"
#include "hashtray_assert.h"

#if defined(REMEMBER_COLLISIONS) || defined(REMEMBER_LOSS)
#include <assert.h>
#endif

#if defined(MULTIPROCESS)
#include <sys/mman.h>
#endif

#include "hashtray_debug.h"
#include "hashtray_hash.h"
#include "lock.h"

typedef struct index_pair_t index_pair_t;
typedef struct entry_t entry_t;
typedef struct cell_t cell_t;
typedef struct compute_result_t compute_result_t;

struct index_pair_t {
  hashtray_key_t idx[CHOICES];
};

struct entry_t {
  bool clear;
  hashtray_key_t key;
  hashtray_value_t value;
};

struct cell_t {
  entry_t entry[NUM_CELL_ENTRIES];
};

struct hashtray_table_t {
  cell_t cell[TABLE_SIZE];
  lock_t lock[TABLE_SIZE];
};

struct compute_result_t {
  index_pair_t indices;
  hashtray_key_t fingerprint;
};

#if defined(REMEMBER_LOSS)
struct hashtray_overfill_t {
  entry_t entry[NUM_OVERFILL_ENTRIES];
};
#endif

#if defined(REMEMBER_COLLISIONS)
struct hashtray_collision_t {
  entry_t entry[NUM_COLLIDED_ENTRIES];
  entry_t collided_with[NUM_COLLIDED_ENTRIES];
};
#endif

#if defined(REMEMBER_LOSS)
hashtray_overfill_t hashtray_overfill;
int32_t hashtray_overfill_idx = 0;
#endif

#if defined(REMEMBER_COLLISIONS)
hashtray_collision_t hashtray_collision;
int32_t hashtray_collision_idx = 0;
#endif

static hashtray_key_t alt_idx(hashtray_key_t idx, hashtray_key_t fp) {
  hashtray_key_t h1 = hashtray_hash_key(0, fp);
  hashtray_key_t h2 = hashtray_hash_key(1, fp);
  if (idx == h1) {
    return h2;
  }
  hashtray_assert(idx == h2);
  return h1;
}

static compute_result_t compute_indices(hashtray_data_t data) {
  compute_result_t result;
  result.fingerprint = hashtray_fingerprint(data);
  for (int32_t i = 0; i < CHOICES; i++) {
    result.indices.idx[i] = hashtray_hash_key(i, result.fingerprint);
  }
  for (int32_t i = 0; i < CHOICES; i++) {
    hashtray_assert((int32_t) result.indices.idx[i] >= 0);
    hashtray_assert((int32_t) result.indices.idx[i] < TABLE_SIZE);
  }
  return result;
}

static inline void lock_indices(hashtray_table_t *t, index_pair_t is) {
#if !defined(MULTITHREADED) && !defined(MULTIPROCESS)
  (void)t;
  (void)is;
#else
  unsigned int seed = (unsigned int)(uintptr_t)&t ^ (unsigned int)(uintptr_t)&is;
  while (true) {
    int32_t idx = 0;
    for (; idx < CHOICES; idx++) {
      if (!hashtray_lock_try(&t->lock[(int32_t) is.idx[idx]])) {
        for (int32_t prev = 0; prev < idx; prev++) {
          hashtray_lock_release(&t->lock[(int32_t) is.idx[prev]]);
        }
        struct timespec req, rem;
        req.tv_sec = 0;
        req.tv_nsec = 1000 * ((unsigned int)rand_r(&seed) % BACKOFF_SLEEP_MICROSEC);
        while (nanosleep(&req, &rem) != 0) {
          req = rem;
        }
        break;
      }
    }
    if (idx == CHOICES) {
      break;
    }
  }
#endif
}

static inline void unlock_indices_except(
  hashtray_table_t *t,
  index_pair_t is,
  int32_t *opt_keep_locked
) {
#if !defined(MULTITHREADED) && !defined(MULTIPROCESS)
  (void)t;
  (void)is;
  (void)opt_keep_locked;
#else
  for (int32_t idx = 0; idx < CHOICES; idx++) {
    int32_t table_idx = (int32_t) is.idx[idx];
    if (opt_keep_locked == NULL || *opt_keep_locked != table_idx) {
      hashtray_lock_release(&t->lock[table_idx]);
    }
  }
#endif
}

#if defined(REMEMBER_LOSS)
void hashtray_print_overfill(bool show_entries) {
  assert(hashtray_overfill_idx >= 0);
  printf("overfill_idx=%d\n", hashtray_overfill_idx);
  if (show_entries) {
    for (int32_t idx = 0; idx < hashtray_overfill_idx; idx++) {
      printf("%d\t", idx);
      printf("key=%d\t", hashtray_overfill.entry[idx].key);
      printf("value=%d\n", hashtray_overfill.entry[idx].value);
    }
  }
}

void hashtray_reset_overfill(void) {
  hashtray_overfill_idx = 0;
}

bool hashtray_has_overflowed(hashtray_data_t data) {
  for (int32_t idx = 0; idx < hashtray_overfill_idx; idx++) {
    if (data == hashtray_overfill.entry[idx].key) {
      return true;
    }
  }
  return false;
}
#endif

#if defined(REMEMBER_COLLISIONS)
void hashtray_print_collision(bool show_entries) {
  assert(hashtray_collision_idx >= 0);
  printf("collision_idx=%d\n", hashtray_collision_idx);
  if (show_entries) {
    for (int32_t idx = 0; idx < hashtray_collision_idx; idx++) {
      printf("%d\t", idx);
      printf("key=%d\t", hashtray_collision.entry[idx].key);
      printf("value=%d\t", hashtray_collision.entry[idx].value);
      printf("collided_key=%d\t", hashtray_collision.collided_with[idx].key);
      printf("collided_value=%d\n", hashtray_collision.collided_with[idx].value);
    }
  }
}

void hashtray_reset_collision(void) {
  hashtray_collision_idx = 0;
}

bool hashtray_has_collided(hashtray_data_t data, hashtray_value_t queried_metadata) {
  assert(hashtray_collision_idx > 0);
  hashtray_key_t fp = hashtray_fingerprint(data);
  for (int32_t idx = 0; idx < hashtray_collision_idx; idx++) {
    if (fp == hashtray_collision.entry[idx].key
      || fp == hashtray_collision.collided_with[idx].key) {
      if (queried_metadata == hashtray_collision.entry[idx].value
        || queried_metadata == hashtray_collision.collided_with[idx].value) {
        return true;
      }
    }
  }
  return false;
}
#endif

bool hashtray_insert(
  hashtray_table_t *t,
  hashtray_data_t data,
  hashtray_data_t metadata,
  hashtray_merge_fn_t *merge_fn,
  hashtray_expiry_fn_t *expiry_fn
) {
  compute_result_t cr = compute_indices(data);
  hashtray_key_t fp = cr.fingerprint;
  index_pair_t is = cr.indices;

  #if defined(HASHTRAY_LOG_INSERTS)
  printf("data=%u\t", data);
  printf("metadata=%d\t", metadata);
  printf("fingerprint=%d\t", fp);
  printf("is.idx[0]=%d\t", is.idx[0]);
  printf("is.idx[1]=%d\n", is.idx[1]);
  #endif

  lock_indices(t, is);

  #if defined(REMEMBER_COLLISIONS)
  for (int32_t idx = 0; idx < CHOICES; idx++) {
    int32_t ti = (int32_t) is.idx[idx];
    for (int32_t i = 0; i < NUM_CELL_ENTRIES; i++) {
      if (!t->cell[ti].entry[i].clear
        && t->cell[ti].entry[i].key == fp) {
        hashtray_assert(hashtray_collision_idx < NUM_COLLIDED_ENTRIES);
        hashtray_collision.entry[hashtray_collision_idx].key = fp;
        hashtray_collision.entry[hashtray_collision_idx].value = metadata;
        hashtray_collision.collided_with[hashtray_collision_idx].key
          = t->cell[ti].entry[i].key;
        hashtray_collision.collided_with[hashtray_collision_idx].value
          = t->cell[ti].entry[i].value;
        #if defined(HASHTRAY_DESCRIBE_COLLISIONS)
        printf("(%d, %d)", hashtray_collision.entry[hashtray_collision_idx].key, hashtray_collision.entry[hashtray_collision_idx].value);
        printf(" collided with ");
        printf("(%d, %d)", hashtray_collision.collided_with[hashtray_collision_idx].key, hashtray_collision.collided_with[hashtray_collision_idx].value);
        printf(" on table_idx=%d, entry=%d\n", ti, i);
        #endif
        hashtray_collision_idx += 1;
      }
    }
  }
  #endif

  bool exists = false;
  int32_t table_idx = 0;
  int32_t entry_idx = 0;
  bool found_free = false;
  int32_t free_table_idx = 0;
  int32_t free_entry_idx = 0;

  for (int32_t idx = 0; idx < CHOICES; idx++) {
    table_idx = (int32_t) is.idx[idx];
    for (entry_idx = 0; entry_idx < NUM_CELL_ENTRIES; entry_idx++) {
      if (t->cell[table_idx].entry[entry_idx].clear && !found_free) {
        found_free = true;
        free_table_idx = table_idx;
        free_entry_idx = entry_idx;
      }
      if (!t->cell[table_idx].entry[entry_idx].clear
        && t->cell[table_idx].entry[entry_idx].key == fp) {
        exists = true;
        break;
      }
    }
    if (exists) {
      break;
    }
  }

  int32_t should_delete = 0;

  if (exists) {
    hashtray_assert(!t->cell[table_idx].entry[entry_idx].clear);
    hashtray_assert(t->cell[table_idx].entry[entry_idx].key == fp);
    if (merge_fn == NULL) {
      t->cell[table_idx].entry[entry_idx].value = metadata;
    } else {
      should_delete = merge_fn(
        &(t->cell[table_idx].entry[entry_idx].value),
        &metadata
      );
      if (should_delete != 0) {
        t->cell[table_idx].entry[entry_idx].clear = true;
      }
    }
  } else if (found_free) {
    t->cell[free_table_idx].entry[free_entry_idx].clear = false;
    t->cell[free_table_idx].entry[free_entry_idx].key = fp;
    t->cell[free_table_idx].entry[free_entry_idx].value = metadata;
  }

  if (exists || found_free) {
    unlock_indices_except(t, is, NULL);
    return true;
  }

  #if defined(HASHTRAY_FAIL_EAGERLY)
  unlock_indices_except(t, is, NULL);
  return false;
  #else
  unsigned int evict_seed = (unsigned int)fp ^ (unsigned int)data;
  table_idx = (int32_t) is.idx[(int32_t)((unsigned int)rand_r(&evict_seed) % CHOICES)];
  unlock_indices_except(t, is, &table_idx);

  hashtray_key_t swapped_key;
  hashtray_value_t swapped_value;

  for (int32_t try_num = 0; try_num < MAX_KICKOUTS; try_num++) {
    int32_t entry = (int32_t)((unsigned int)rand_r(&evict_seed) % NUM_CELL_ENTRIES);

    if (expiry_fn != NULL
      && expiry_fn(&(t->cell[table_idx].entry[entry].value)) == 0) {
      continue;
    }

    swapped_key = t->cell[table_idx].entry[entry].key;
    swapped_value = t->cell[table_idx].entry[entry].value;
    t->cell[table_idx].entry[entry].key = fp;
    t->cell[table_idx].entry[entry].value = metadata;

    if (t->cell[table_idx].entry[entry].clear) {
      t->cell[table_idx].entry[entry].clear = false;
      hashtray_lock_release(&t->lock[table_idx]);
      return true;
    }

    fp = swapped_key;
    metadata = swapped_value;
    hashtray_lock_release(&t->lock[table_idx]);
    table_idx = (int32_t) alt_idx((hashtray_key_t) table_idx, fp);
    hashtray_lock_acquire(&t->lock[table_idx]);
  }

  #if defined(REMEMBER_LOSS)
  assert(hashtray_overfill_idx < NUM_OVERFILL_ENTRIES);
  hashtray_overfill.entry[hashtray_overfill_idx].clear = false;
  hashtray_overfill.entry[hashtray_overfill_idx].key = fp;
  hashtray_overfill.entry[hashtray_overfill_idx].value = metadata;
  hashtray_overfill_idx += 1;
  #endif
  return false;
  #endif
}

bool hashtray_remove(hashtray_table_t *t, hashtray_data_t data) {
  bool found = false;
  compute_result_t cr = compute_indices(data);
  hashtray_key_t fp = cr.fingerprint;
  index_pair_t is = cr.indices;
  lock_indices(t, is);

  for (int32_t idx = 0; idx < CHOICES; idx++) {
    int32_t ti = (int32_t) is.idx[idx];
    for (int32_t i = 0; i < NUM_CELL_ENTRIES; i++) {
      if (!t->cell[ti].entry[i].clear && t->cell[ti].entry[i].key == fp) {
        t->cell[ti].entry[i].clear = true;
        found = true;
        break;
      }
    }
    if (found) {
      break;
    }
  }

  unlock_indices_except(t, is, NULL);
  return found;
}

bool hashtray_contains(hashtray_table_t *t, hashtray_data_t data) {
  bool found = false;
  compute_result_t cr = compute_indices(data);
  hashtray_key_t fp = cr.fingerprint;
  index_pair_t is = cr.indices;
  lock_indices(t, is);

  for (int32_t idx = 0; idx < CHOICES; idx++) {
    int32_t ti = (int32_t) is.idx[idx];
    for (int32_t i = 0; i < NUM_CELL_ENTRIES; i++) {
      if (!t->cell[ti].entry[i].clear && t->cell[ti].entry[i].key == fp) {
        found = true;
        break;
      }
    }
    if (found) {
      break;
    }
  }

  unlock_indices_except(t, is, NULL);
  return found;
}

hashtray_data_t hashtray_lookup(
  hashtray_table_t *t,
  hashtray_data_t data,
  hashtray_apply_fn_t *apply_fn
) {
  hashtray_data_t result = 0;
  bool done = false;
  compute_result_t cr = compute_indices(data);
  hashtray_key_t fp = cr.fingerprint;
  index_pair_t is = cr.indices;
  lock_indices(t, is);

  for (int32_t idx = 0; idx < CHOICES; idx++) {
    int32_t ti = (int32_t) is.idx[idx];
    for (int32_t i = 0; i < NUM_CELL_ENTRIES; i++) {
      if (!t->cell[ti].entry[i].clear && t->cell[ti].entry[i].key == fp) {
        int32_t should_delete = 0;
        if (apply_fn != NULL) {
          should_delete = apply_fn(&(t->cell[ti].entry[i].value));
        }

        if (should_delete != 0) {
          t->cell[ti].entry[i].clear = true;
        } else {
          result = t->cell[ti].entry[i].value;
        }
        done = true;
        break;
      }
    }
    if (done) {
      break;
    }
  }

  unlock_indices_except(t, is, NULL);
  return result;
}

hashtray_table_t *hashtray_create_table(void) {
  #if defined(MULTIPROCESS)
  hashtray_table_t *t = mmap(
    NULL,
    sizeof(*t),
    PROT_READ | PROT_WRITE,
    MAP_ANONYMOUS | MAP_SHARED,
    -1,
    0
  );
  hashtray_assert(t != MAP_FAILED);
  #else
  hashtray_table_t *t = malloc(sizeof(*t));
  #endif

  for (int32_t ti = 0; ti < TABLE_SIZE; ti++) {
    for (int32_t i = 0; i < NUM_CELL_ENTRIES; i++) {
      t->cell[ti].entry[i].clear = true;
    }
    t->lock[ti] = hashtray_lock_init(ti);
  }
  return t;
}

void hashtray_destroy_table(hashtray_table_t *t) {
  for (int32_t ti = 0; ti < TABLE_SIZE; ti++) {
    hashtray_lock_destroy(&t->lock[ti], ti);
  }

  #if defined(MULTIPROCESS)
  hashtray_assert(!munmap(t, sizeof(*t)));
  #else
  free(t);
  #endif
}

hashtray_serialised_t hashtray_serialise_table(hashtray_table_t *t) {
  const int32_t buffer_size = TABLE_SIZE * NUM_CELL_ENTRIES * (int32_t) sizeof(entry_t);
  char *buffer = malloc((size_t) buffer_size);
  if (buffer == NULL) {
    return (hashtray_serialised_t){NULL, -1};
  }

  int32_t idx = 0;
  for (int32_t ti = 0; ti < TABLE_SIZE; ti++) {
    hashtray_lock_acquire(&t->lock[ti]);
    for (int32_t ei = 0; ei < NUM_CELL_ENTRIES; ei++) {
      memcpy(buffer + idx, &(t->cell[ti].entry[ei]), sizeof(entry_t));
      idx += (int32_t) sizeof(entry_t);
    }
    hashtray_lock_release(&t->lock[ti]);
  }

  return (hashtray_serialised_t){buffer, buffer_size};
}

int32_t hashtray_deserialise_table(
  hashtray_table_t *t,
  int32_t buffer_len,
  const char *buffer
) {
  if (t == NULL || buffer == NULL) {
    return -1;
  }
  const int32_t buffer_size = TABLE_SIZE * NUM_CELL_ENTRIES * (int32_t) sizeof(entry_t);
  if (buffer_size != buffer_len) {
    return -1;
  }

  int32_t idx = 0;
  for (int32_t ti = 0; ti < TABLE_SIZE; ti++) {
    hashtray_lock_acquire(&t->lock[ti]);
    for (int32_t ei = 0; ei < NUM_CELL_ENTRIES; ei++) {
      memcpy(&(t->cell[ti].entry[ei]), buffer + idx, sizeof(entry_t));
      idx += (int32_t) sizeof(entry_t);
    }
    hashtray_lock_release(&t->lock[ti]);
  }

  return 0;
}

hashtray_key_array_t hashtray_keys_of_table(hashtray_table_t *t) {
  const int32_t max_entries = TABLE_SIZE * NUM_CELL_ENTRIES;
  hashtray_key_t *buffer = malloc((size_t) max_entries * sizeof(hashtray_key_t));
  if (buffer == NULL) {
    return (hashtray_key_array_t){NULL, -1};
  }

  uint32_t count = 0;
  for (int32_t ti = 0; ti < TABLE_SIZE; ti++) {
    hashtray_lock_acquire(&t->lock[ti]);
    for (int32_t ei = 0; ei < NUM_CELL_ENTRIES; ei++) {
      if (!t->cell[ti].entry[ei].clear) {
        buffer[count] = t->cell[ti].entry[ei].key;
        count += 1;
      }
    }
    hashtray_lock_release(&t->lock[ti]);
  }

  if (count == 0) {
    free(buffer);
    return (hashtray_key_array_t){NULL, (int32_t) count};
  }

  hashtray_key_t *tmp = realloc(buffer, count * sizeof(hashtray_key_t));
  if (tmp == NULL) {
    free(buffer);
    return (hashtray_key_array_t){NULL, -1};
  }

  return (hashtray_key_array_t){tmp, (int32_t) count};
}

hashtray_value_array_t hashtray_values_of_table(hashtray_table_t *t) {
  const int32_t max_entries = TABLE_SIZE * NUM_CELL_ENTRIES;
  hashtray_value_t *buffer = malloc((size_t) max_entries * sizeof(hashtray_value_t));
  if (buffer == NULL) {
    return (hashtray_value_array_t){NULL, -1};
  }

  uint32_t count = 0;
  for (int32_t ti = 0; ti < TABLE_SIZE; ti++) {
    hashtray_lock_acquire(&t->lock[ti]);
    for (int32_t ei = 0; ei < NUM_CELL_ENTRIES; ei++) {
      if (!t->cell[ti].entry[ei].clear) {
        buffer[count] = t->cell[ti].entry[ei].value;
        count += 1;
      }
    }
    hashtray_lock_release(&t->lock[ti]);
  }

  if (count == 0) {
    free(buffer);
    return (hashtray_value_array_t){NULL, (int32_t) count};
  }

  hashtray_value_t *tmp = realloc(buffer, count * sizeof(hashtray_value_t));
  if (tmp == NULL) {
    free(buffer);
    return (hashtray_value_array_t){NULL, -1};
  }

  return (hashtray_value_array_t){tmp, (int32_t) count};
}

int32_t hashtray_rand_range(int32_t min, int32_t max) {
  hashtray_assert(min >= 0);
  hashtray_assert(max >= min);
  if (min == max) {
    return min;
  }
  return min + (int32_t)((unsigned int)rand() % (unsigned int)(max - min));
}
