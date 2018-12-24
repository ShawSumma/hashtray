/*
Instantiating hashtray API using uthash, for testing.
Nik Sultana, University of Pennsylvania, February 2018
*/

// NOTE I only define this for specific experiments.
//#define INFINITE_TABLE

#ifdef HASHTRAY_UTHASH_DEBUG
#include <stdio.h>
#endif // HASHTRAY_UTHASH_DEBUG

#include "uthash.h"

#ifdef HASHTRAY_ASSERT
#include <assert.h>
#endif // HASHTRAY_ASSERT

#ifndef MULTITHREADED
#error This table instance requires MULTITHREADED
#else
#include <pthread.h>
#endif // MULTITHREADED

struct entry {
   HASHTRAY(data_t) key; // NOTE not using HASHTRAY(key_t) since that's the type of fingerprints
   HASHTRAY(value_t) value;
   UT_hash_handle hh;
};

struct HASHTRAY(table) {
  struct entry * table;
  pthread_mutex_t lock;
};

const char * HASHTRAY(outcome_str)[] =
  {"OK", "NOT_FOUND", "GAVE_UP", "BLOCKS_FULL"};

enum HASHTRAY(outcome)
HASHTRAY(insert)(struct HASHTRAY(table) * t, HASHTRAY(data_t) data, HASHTRAY(data_t) metadata,
   int (*merge_fun)(HASHTRAY(data_t) * stored, const HASHTRAY(data_t) * new),
   int (*expiry_fun)(const HASHTRAY(data_t) * metadata))
{
#ifdef HASHTRAY_ASSERT
  // This feature is not supported for this hashtable instance.
  assert (NULL == merge_fun);
  assert (NULL == expiry_fun);
#endif // HASHTRAY_ASSERT

  int error = pthread_mutex_lock(&(t->lock));
#ifdef HASHTRAY_ASSERT
  assert(!error); // FIXME check when !HASHTRAY_ASSERT
#endif // HASHTRAY_ASSERT

#ifndef INFINITE_TABLE
  // Enforce TABLE_SIZE unless modelling perfection
  if (HASH_COUNT(t->table) > TABLE_SIZE) {

// NOTE i'm leaving this commented code here in case it's useful
//      in future tests. The point is that the table can be larger than
//      TABLE_SIZE -- this first test checks to ensure that TABLE_SIZE
//      has a specific value:
//if (TABLE_SIZE != 100) { free(-1); /*FIXME crude test*/ }
//      Moreover, we can enforce a larger table size (than TABLE_SIZE)
//      to retain more control than we'd have with INFINITE_TABLE.
//      I do this to check that finite but larget values of network size
//      (measured in hosts) behaves as intended -- results can be strange
//      because of the "catching up" effect when simulations need longer time
//      (when they involve a larger number of hosts).
//      So here we can enforce that size, and can decide when and how to
//      evict when the table can't take any more entries:
//  if (HASH_COUNT(t->table) > 400/*FIXME const*/) {
//  if (HASH_COUNT(t->table) > 400000/*FIXME const*/) {
//  if (HASH_COUNT(t->table) > 800000/*FIXME const*/) {

    // Simple eviction policy:
#ifdef HASHTRAY_ASSERT
    assert (NULL != t->table);
#endif // HASHTRAY_ASSERT
    struct entry * first = t->table;
    HASH_DEL(t->table, first); // Remove first element in the table
    free(first);
  }
#endif // INFINITE_TABLE

#ifdef HASHTRAY_UTHASH_DEBUG
  printf("=%d\n", HASH_COUNT(t->table));
#endif // HASHTRAY_UTHASH_DEBUG
  struct entry * record = malloc(sizeof(*record));
  record->key = data;
  record->value = metadata;

  HASH_ADD_INT/*FIXME assume specific type of key*/(t->table, key, record);

  error = pthread_mutex_unlock(&(t->lock));
#ifdef HASHTRAY_ASSERT
  assert(!error); // FIXME check when !HASHTRAY_ASSERT
#endif // HASHTRAY_ASSERT

  return HASHTRAY(OK); // FIXME const
}

enum HASHTRAY(outcome)
HASHTRAY(delete)(struct HASHTRAY(table) * t, HASHTRAY(data_t) data)
{
#ifdef HASHTRAY_ASSERT
  // This feature is not supported for this hashtable instance.
  assert (0);
#endif // HASHTRAY_ASSERT
  free(-1);
  return HASHTRAY(OK); // NOTE control shouldn't arrive here.
}

enum HASHTRAY(outcome)
HASHTRAY(lookup)(struct HASHTRAY(table) * t, HASHTRAY(data_t) data, HASHTRAY(data_t) * metadata,
    int (*apply_fun)(HASHTRAY(data_t) * metadata))
{
#ifdef HASHTRAY_ASSERT
  // This feature is not supported for this hashtable instance.
  assert (NULL == apply_fun);
#endif // HASHTRAY_ASSERT

  enum HASHTRAY(outcome) result = HASHTRAY(NOT_FOUND);
  struct entry * retrieved = NULL;

  int error = pthread_mutex_lock(&(t->lock));
#ifdef HASHTRAY_ASSERT
  assert(!error); // FIXME check when !HASHTRAY_ASSERT
#endif // HASHTRAY_ASSERT

#ifdef HASHTRAY_UTHASH_DEBUG
  printf("[%d\n", HASH_COUNT(t->table));
#endif // HASHTRAY_UTHASH_DEBUG
  HASH_FIND_INT/*FIXME assume specific type of key*/(t->table, &data, retrieved);
  if (NULL != retrieved) {
    *metadata = retrieved->value;
    result = HASHTRAY(OK);
  }

  error = pthread_mutex_unlock(&(t->lock));
#ifdef HASHTRAY_ASSERT
  assert(!error); // FIXME check when !HASHTRAY_ASSERT
#endif // HASHTRAY_ASSERT

  return result;
}

struct HASHTRAY(table) *
HASHTRAY(create_table)(void)
{
  struct HASHTRAY(table) * result = malloc(sizeof(*result));
  result->table = NULL;

  int error = pthread_mutex_init(&(result->lock), NULL);
#ifdef HASHTRAY_ASSERT
  assert(!error); // FIXME check when !HASHTRAY_ASSERT
#endif // HASHTRAY_ASSERT

  return result;
}

void
HASHTRAY(destroy_table)(struct HASHTRAY(table) * t)
{
  int error = pthread_mutex_destroy(&(t->lock));
#ifdef HASHTRAY_ASSERT
  assert(!error); // FIXME check when !HASHTRAY_ASSERT
#endif // HASHTRAY_ASSERT

  // uthash table is destroyed automatically by uthash when it's vacated, so should simply vacate it.
  struct entry * cursor;
  struct entry * tmp;
  HASH_ITER(hh, t->table, cursor, tmp) {
    HASH_DEL(t->table, cursor);
    free(cursor);
  }

  free(t);
}

int
HASHTRAY(serialise_table)(struct HASHTRAY(table) * t, char ** buffer)
{
  const int buffer_size = TABLE_SIZE * NUM_CELL_ENTRIES * sizeof(struct entry);
  *buffer = malloc(buffer_size);
  if (NULL == *buffer) {
    return -1;
  }

  int idx = 0;

  struct entry * cursor;
  struct entry * tmp;
  HASH_ITER(hh, t->table, cursor, tmp) {
    memcpy(*buffer + idx, cursor, sizeof(struct entry));
    idx += sizeof(struct entry);
  }

  return buffer_size;
}

int
HASHTRAY(deserialise_table)(const char * buffer, const int buffer_len, struct HASHTRAY(table) * t)
{
  if (NULL == buffer) {
    return -1;
  }
  const int buffer_size = TABLE_SIZE * NUM_CELL_ENTRIES * sizeof(struct entry);
  if (buffer_size != buffer_len) {
    return -1;
  }

  int idx = 0;

  struct entry * cursor;
  struct entry * tmp;
  HASH_ITER(hh, t->table, cursor, tmp) {
    memcpy(cursor, buffer + idx, sizeof(struct entry));
    idx += sizeof(struct entry);
  }

  return 0;
}

int
HASHTRAY(rand_range)(int min, int max)
{
#ifdef HASHTRAY_ASSERT
  assert(min >= 0);
  assert(max >= min);
#endif // HASHTRAY_ASSERT

  if (min == max) {
    return min;
  }

  return min + (rand() % (max - min));
}
