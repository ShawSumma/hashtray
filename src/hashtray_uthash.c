// Hashtray API implemented via uthash, for testing.

#include <stdlib.h>

#if defined(HASHTRAY_UTHASH_DEBUG)
#include <stdio.h>
#endif

#include "uthash.h"

#include "hashtray.h"
#include "hashtray_assert.h"

#if !defined(MULTITHREADED)
#error This table instance requires MULTITHREADED
#else
#include <pthread.h>
#endif

typedef struct uthash_entry_t uthash_entry_t;

struct uthash_entry_t {
  hashtray_data_t key;
  hashtray_value_t value;
  UT_hash_handle hh;
};

struct hashtray_table_t {
  uthash_entry_t *table;
  pthread_mutex_t lock;
};

bool hashtray_insert(
  hashtray_table_t *t,
  hashtray_data_t data,
  hashtray_data_t metadata,
  hashtray_merge_fn_t *merge_fn,
  hashtray_expiry_fn_t *expiry_fn
) {
  hashtray_assert(merge_fn == NULL);
  hashtray_assert(expiry_fn == NULL);

  hashtray_assert(!pthread_mutex_lock(&(t->lock)));

  #if !defined(INFINITE_TABLE)
  if (HASH_COUNT(t->table) > TABLE_SIZE) {
    hashtray_assert(t->table != NULL);
    uthash_entry_t *first = t->table;
    HASH_DEL(t->table, first);
    free(first);
  }
  #endif

  #if defined(HASHTRAY_UTHASH_DEBUG)
  printf("=%d\n", HASH_COUNT(t->table));
  #endif
  uthash_entry_t *record = malloc(sizeof(*record));
  record->key = data;
  record->value = metadata;

  HASH_ADD_INT(t->table, key, record);

  hashtray_assert(!pthread_mutex_unlock(&(t->lock)));

  return true;
}

bool hashtray_remove(hashtray_table_t *t, hashtray_data_t data) {
  hashtray_assert(false);
  (void)t;
  (void)data;
  abort();
  return false;
}

bool hashtray_contains(hashtray_table_t *t, hashtray_data_t data) {
  hashtray_assert(!pthread_mutex_lock(&(t->lock)));

  uthash_entry_t *retrieved = NULL;
  HASH_FIND_INT(t->table, &data, retrieved);
  bool found = (retrieved != NULL);

  hashtray_assert(!pthread_mutex_unlock(&(t->lock)));

  return found;
}

hashtray_data_t hashtray_lookup(
  hashtray_table_t *t,
  hashtray_data_t data,
  hashtray_apply_fn_t *apply_fn
) {
  hashtray_assert(apply_fn == NULL);

  hashtray_data_t result = 0;
  uthash_entry_t *retrieved = NULL;

  hashtray_assert(!pthread_mutex_lock(&(t->lock)));

  #if defined(HASHTRAY_UTHASH_DEBUG)
  printf("[%d\n", HASH_COUNT(t->table));
  #endif
  HASH_FIND_INT(t->table, &data, retrieved);
  if (retrieved != NULL) {
    result = retrieved->value;
  }

  hashtray_assert(!pthread_mutex_unlock(&(t->lock)));

  return result;
}

hashtray_table_t *hashtray_create_table(void) {
  hashtray_table_t *result = malloc(sizeof(*result));
  result->table = NULL;

  hashtray_assert(!pthread_mutex_init(&(result->lock), NULL));

  return result;
}

void hashtray_destroy_table(hashtray_table_t *t) {
  hashtray_assert(!pthread_mutex_destroy(&(t->lock)));

  uthash_entry_t *cursor;
  uthash_entry_t *tmp;
  HASH_ITER(hh, t->table, cursor, tmp) {
    HASH_DEL(t->table, cursor);
    free(cursor);
  }

  free(t);
}

hashtray_serialised_t hashtray_serialise_table(hashtray_table_t *t) {
  const int32_t buffer_size = TABLE_SIZE * NUM_CELL_ENTRIES * (int32_t) sizeof(uthash_entry_t);
  char *buffer = malloc((size_t) buffer_size);
  if (buffer == NULL) {
    return (hashtray_serialised_t){NULL, -1};
  }

  int32_t idx = 0;

  uthash_entry_t *cursor;
  uthash_entry_t *tmp;
  HASH_ITER(hh, t->table, cursor, tmp) {
    memcpy(buffer + idx, cursor, sizeof(uthash_entry_t));
    idx += (int32_t) sizeof(uthash_entry_t);
  }

  return (hashtray_serialised_t){buffer, buffer_size};
}

int32_t hashtray_deserialise_table(
  hashtray_table_t *t,
  int32_t buffer_len,
  const char *buffer
) {
  if (buffer == NULL) {
    return -1;
  }
  const int32_t buffer_size = TABLE_SIZE * NUM_CELL_ENTRIES * (int32_t) sizeof(uthash_entry_t);
  if (buffer_size != buffer_len) {
    return -1;
  }

  int32_t idx = 0;

  uthash_entry_t *cursor;
  uthash_entry_t *tmp;
  HASH_ITER(hh, t->table, cursor, tmp) {
    memcpy(cursor, buffer + idx, sizeof(uthash_entry_t));
    idx += (int32_t) sizeof(uthash_entry_t);
  }

  return 0;
}

int32_t hashtray_rand_range(int32_t min, int32_t max) {
  hashtray_assert(min >= 0);
  hashtray_assert(max >= min);

  if (min == max) {
    return min;
  }

  return min + (rand() % (max - min));
}
