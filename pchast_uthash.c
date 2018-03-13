/*
Instantiating pchast API using uthash, for testing.
Nik Sultana, University of Pennsylvania, February 2018
*/

// NOTE I only define this for specific experiments.
//#define INFINITE_TABLE

#ifdef PCHAST_UTHASH_DEBUG
#include <stdio.h>
#endif // PCHAST_UTHASH_DEBUG

#include "uthash.h"

#ifdef PCHAST_ASSERT
#include <assert.h>
#endif // PCHAST_ASSERT

#ifndef MULTITHREADED
#error This table instance requires MULTITHREADED
#else
#include <pthread.h>
#endif // MULTITHREADED

struct entry {
   PCH(data_t) key; // NOTE not using PCH(key_t) since that's the type of fingerprints
   PCH(value_t) value;
   UT_hash_handle hh;
};

struct PCH(table) {
  struct entry * table;
  pthread_mutex_t lock;
};

const char * PCH(outcome_str)[] =
  {"OK", "NOT_FOUND", "GAVE_UP", "BLOCKS_FULL"};

enum PCH(outcome)
PCH(insert)(struct PCH(table) * t, PCH(data_t) data, PCH(data_t) metadata,
   int (*merge_fun)(PCH(data_t) * stored, const PCH(data_t) * new),
   int (*expiry_fun)(const PCH(data_t) * metadata))
{
#ifdef PCHAST_ASSERT
  // This feature is not supported for this hashtable instance.
  assert (NULL == merge_fun);
  assert (NULL == expiry_fun);
#endif // PCHAST_ASSERT

  int error = pthread_mutex_lock(&(t->lock));
#ifdef PCHAST_ASSERT
  assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT

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
#ifdef PCHAST_ASSERT
    assert (NULL != t->table);
#endif // PCHAST_ASSERT
    struct entry * first = t->table;
    HASH_DEL(t->table, first); // Remove first element in the table
    free(first);
  }
#endif // INFINITE_TABLE

#ifdef PCHAST_UTHASH_DEBUG
  printf("=%d\n", HASH_COUNT(t->table));
#endif // PCHAST_UTHASH_DEBUG
  struct entry * record = malloc(sizeof(*record));
  record->key = data;
  record->value = metadata;

  HASH_ADD_INT/*FIXME assume specific type of key*/(t->table, key, record);

  error = pthread_mutex_unlock(&(t->lock));
#ifdef PCHAST_ASSERT
  assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT

  return PCH(OK); // FIXME const
}

enum PCH(outcome)
PCH(delete)(struct PCH(table) * t, PCH(data_t) data)
{
#ifdef PCHAST_ASSERT
  // This feature is not supported for this hashtable instance.
  assert (0);
#endif // PCHAST_ASSERT
  free(-1);
  return PCH(OK); // NOTE control shouldn't arrive here.
}

enum PCH(outcome)
PCH(lookup)(struct PCH(table) * t, PCH(data_t) data, PCH(data_t) * metadata,
    int (*apply_fun)(PCH(data_t) * metadata))
{
#ifdef PCHAST_ASSERT
  // This feature is not supported for this hashtable instance.
  assert (NULL == apply_fun);
#endif // PCHAST_ASSERT

  enum PCH(outcome) result = PCH(NOT_FOUND);
  struct entry * retrieved = NULL;

  int error = pthread_mutex_lock(&(t->lock));
#ifdef PCHAST_ASSERT
  assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT

#ifdef PCHAST_UTHASH_DEBUG
  printf("[%d\n", HASH_COUNT(t->table));
#endif // PCHAST_UTHASH_DEBUG
  HASH_FIND_INT/*FIXME assume specific type of key*/(t->table, &data, retrieved);
  if (NULL != retrieved) {
    *metadata = retrieved->value;
    result = PCH(OK);
  }

  error = pthread_mutex_unlock(&(t->lock));
#ifdef PCHAST_ASSERT
  assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT

  return result;
}

struct PCH(table) *
PCH(create_table)(void)
{
  struct PCH(table) * result = malloc(sizeof(*result));
  result->table = NULL;

  int error = pthread_mutex_init(&(result->lock), NULL);
#ifdef PCHAST_ASSERT
  assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT

  return result;
}

void
PCH(destroy_table)(struct PCH(table) * t)
{
  int error = pthread_mutex_destroy(&(t->lock));
#ifdef PCHAST_ASSERT
  assert(!error); // FIXME check when !PCHAST_ASSERT
#endif // PCHAST_ASSERT

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
