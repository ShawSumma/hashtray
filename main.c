/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017

NOTE parts of this code are tightly coupled with an amd64 ISA, specifically to
     poll the CPU's time-stamp counter.

TODO
* "Fast mode" vs "thorough mode" when checking blocks.
* Expiry of records.
* Measure the accuracy of recall -- whether items get overwritten or their
  fingerprints collide.
* Implement and measure an "update" function for when an item should be
  reclassified.
*/

#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pchast.h"
#include "randomlib.h"

#define TARGET_CORE 0/*FIXME const*/

#define COOL_THE_CACHE 0
#define CACHE_COOLING_BLOCK 5000000

int
cool_cache(void)
{
  int nonse = 0;
  char * buf = malloc(sizeof(*buf) * CACHE_COOLING_BLOCK);
  for (int i = 0; i < CACHE_COOLING_BLOCK; i++) {
    buf[i] += 1;
    nonse += buf[i];
  }
  return nonse;
}

void
simple_test(DATA_TYPE data, DATA_TYPE metadata)
{
  printf("simple_test: create table, insert data, query for that data, delete that data, re-delete that data, re-query for that data, destroy table.\n");
  table * my_tab = create_table();
  enum outcome o;
  o = insert(my_tab, data, metadata);
  assert(OK == o);

  DATA_TYPE queried_metadata = 0;
  o = lookup(my_tab, data, &queried_metadata);
  assert(OK == o);
  assert(queried_metadata == metadata);

  o = delete(my_tab, data);
  assert(OK == o);

  o = delete(my_tab, data);
  assert(NOT_FOUND == o);
  o = lookup(my_tab, data, &queried_metadata);
  assert(NOT_FOUND == o);

  destroy_table(my_tab);
}

// Based on https://stackoverflow.com/questions/14783782/which-inline-assembly-code-is-correct-for-rdtscp#14783909
static inline uint64_t rdtscp(uint32_t * aux)
{
    uint64_t rax,rdx;
    __asm volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
    return (rdx << 32) + rax;
}

#define TEST_DATASET_SIZE 500
struct test_data {
  uint32_t datum;
  uint32_t metadatum;
};

static inline void
update_stats(int iteration, uint64_t time_before, uint64_t time_after,
  uint64_t * average, uint64_t * max, uint64_t * min)
{
  uint64_t x = time_after - time_before;
  if (0 == iteration ||
      (0 == *average && 0 == *max && 0 == *min)) {
    *average = x;
    *max = x;
    *min = x;
  } else {
    *average = (*average + x) >> 1;
    *max = x > *max ? x : *max;
    *min = x < *min ? x : *min;
  }
}

void
lookup_test(struct test_data * test_dataset, table * test_table)
{
  uint64_t average = 0;
  uint64_t max = 0;
  uint64_t min = 0;

  bool temporary_table = false;
  if (NULL == test_table) {
    test_table = create_table();
    temporary_table = true;
  }

  enum outcome o;

  outcome_count oc;
  RESET_OUTCOME_STATS(oc)

  DATA_TYPE queried_metadata = 0;
  for (int i = 0; i < TEST_DATASET_SIZE; i++) {
    uint32_t data;

    if (NULL != test_dataset) {
      data = test_dataset[i].datum;
    } else {
      data = (uint32_t)RandomInt(0, INT_MAX);
    }

#if COOL_THE_CACHE
    (void)cool_cache();
#endif

    uint32_t aux;
    uint64_t one = rdtscp(&aux);
    o = lookup(test_table, data, &queried_metadata);
    uint64_t two = rdtscp(&aux);
#if 0
    PRINT_OUTCOME(o);
#endif
    if (temporary_table) {
      assert(NOT_FOUND == o);
    } else {
      assert(OK == o || // Assuming that anything in test_dataset appears in test_table.
             NOT_FOUND == o/*..or not FIXME */);  
      if (OK == o) {
        // FIXME This can fail because of collision.  
        //assert(queried_metadata == test_dataset[i].metadatum);
      }
    }
    INCREMENT_OUTCOME(oc, o)
    update_stats(i, one, two, &average, &max, &min);
  }

  if (temporary_table) {
    destroy_table(test_table);
  }

  printf("lookup_test min / average / max duration: %llu / %llu / %llu ticks\n",
      min, average, max);
  PRINT_OUTCOME_STATS(oc)
}

#define METADATA_MAXIMUM 10
struct test_data *
insert_test(table * test_table)
{
  assert(NULL != test_table);

  uint64_t average = 0;
  uint64_t max = 0;
  uint64_t min = 0;

  struct test_data * result = malloc(sizeof(*result) * TEST_DATASET_SIZE);

  enum outcome o;

  outcome_count oc;
  RESET_OUTCOME_STATS(oc)

  for (int i = 0; i < TEST_DATASET_SIZE; i++) {
    result[i].datum = (uint32_t)RandomInt(0, INT_MAX);
    result[i].metadatum = (uint32_t)RandomInt(0, METADATA_MAXIMUM);

#if COOL_THE_CACHE
    (void)cool_cache();
#endif

    uint32_t aux;
    uint64_t one = rdtscp(&aux);
    o = insert(test_table, result[i].datum, result[i].metadatum);
    uint64_t two = rdtscp(&aux);
#if 0
    PRINT_OUTCOME(o);
#endif
    assert(OK == o || // Assuming that table doesn't fill up.
           GAVE_UP == o/* FIXME how easily should we give up?*/);

    INCREMENT_OUTCOME(oc, o)
    update_stats(i, one, two, &average, &max, &min);
  }

  printf("insert_test min / average / max duration: %llu / %llu / %llu ticks\n",
      min, average, max);
  PRINT_OUTCOME_STATS(oc)

  return result;
}

void
mix_insert_lookup_test(void)
{
  uint64_t average_insert = 0;
  uint64_t max_insert = 0;
  uint64_t min_insert = 0;
  outcome_count oc_insert;
  RESET_OUTCOME_STATS(oc_insert)

  uint64_t average_lookup_expectfind = 0;
  uint64_t max_lookup_expectfind = 0;
  uint64_t min_lookup_expectfind = 0;
  outcome_count oc_lookup_expectfind;
  RESET_OUTCOME_STATS(oc_lookup_expectfind)

  uint64_t average_lookup_notexpectfind = 0;
  uint64_t max_lookup_notexpectfind = 0;
  uint64_t min_lookup_notexpectfind = 0;
  outcome_count oc_lookup_notexpectfind;
  RESET_OUTCOME_STATS(oc_lookup_notexpectfind)

  enum {INSERT = 0, INSERT_AND_LOOKUP = 1, LOOKUP = 2} state;

  table * test_table = create_table();
  enum outcome o;

  DATA_TYPE queried_metadata = 0;
  for (int i = 0; i < TEST_DATASET_SIZE; i++) {
    switch (RandomInt(INSERT, LOOKUP)) {
      case 0:
        state = INSERT;
        break;
      case 1:
        state = INSERT_AND_LOOKUP;
        break;
      case 2:
        state = LOOKUP;
        break;
      default:
        assert(0);
    }

    uint32_t data = (uint32_t)RandomInt(0, INT_MAX);

#if COOL_THE_CACHE
    (void)cool_cache();
#endif

    uint32_t aux;
    uint64_t one;
    uint64_t two;

    switch (state) {
    case INSERT:
      one = rdtscp(&aux);
      o = insert(test_table, data, queried_metadata/* FIXME uninitialised */);
      two = rdtscp(&aux);
#if 0
      PRINT_OUTCOME(o);
#endif
//      assert(NOT_FOUND == o ||
//             OK == o /*Allowing for false-positive -- FIXME table should be empty!  
//*/);
      INCREMENT_OUTCOME(oc_insert, o)
      update_stats(i, one, two, &average_insert, &max_insert, &min_insert);
      break;

    case INSERT_AND_LOOKUP:
      (void)insert(test_table, data, queried_metadata/* FIXME uninitialised */);
      one = rdtscp(&aux);
      o = lookup(test_table, data, &queried_metadata);
      two = rdtscp(&aux);
#if 0
      PRINT_OUTCOME(o);
#endif
      assert(NOT_FOUND == o ||
             OK == o /*Allowing for false-positive -- FIXME table should be empty!  
*/);
      INCREMENT_OUTCOME(oc_lookup_expectfind, o)
      update_stats(i, one, two, &average_lookup_expectfind, &max_lookup_expectfind, &min_lookup_expectfind);
      break;

    case LOOKUP:
      one = rdtscp(&aux);
      o = lookup(test_table, data, &queried_metadata);
      two = rdtscp(&aux);
#if 0
      PRINT_OUTCOME(o);
#endif
      assert(NOT_FOUND == o ||
             OK == o /*Allowing for false-positive -- FIXME table should be empty!  
*/);
      INCREMENT_OUTCOME(oc_lookup_notexpectfind, o)
      update_stats(i, one, two, &average_lookup_notexpectfind, &max_lookup_notexpectfind, &min_lookup_notexpectfind);
      break;
    }
  }

  destroy_table(test_table);

  printf("mix_insert_lookup_test:INSERT min / average / max duration: %llu / %llu / %llu ticks\n",
      min_insert, average_insert, max_insert);
  PRINT_OUTCOME_STATS(oc_insert)

  printf("mix_insert_lookup_test:INSERT_AND_LOOKUP min / average / max duration: %llu / %llu / %llu ticks\n",
      min_lookup_expectfind, average_lookup_expectfind, max_lookup_expectfind);
  PRINT_OUTCOME_STATS(oc_lookup_expectfind)

  printf("mix_insert_lookup_test:LOOKUP min / average / max duration: %llu / %llu / %llu ticks\n",
      min_lookup_notexpectfind, average_lookup_notexpectfind, max_lookup_notexpectfind);
  PRINT_OUTCOME_STATS(oc_lookup_notexpectfind)
}

int
main()
{
  printf("Initialising PRNG\n");
  init_prng(PRNG_SEED);
#if 0
  for (int i = 0; i < 10; i++) {
    printf("%d\n", prng());
  }
#endif
  RandomInitialise(1802,9373); // These values were suggested in randomlib.c

#if 0
  // Pin to a single core
  // FIXME doesn't work on macOS
  pthread_t thread_me = pthread_self();
  cpu_set_t cpu_set;
  CPU_SET(TARGET_CORE, &cpu_set);
  assert(0 == pthread_setaffinity_np(thread_me, sizeof(cpu_set_t), &cpu_set));
#endif

  uint32_t aux;
  uint64_t one = rdtscp(&aux);
  simple_test(0, 0);
  uint64_t two = rdtscp(&aux);
  printf("simple_test duration: %lld ticks\n\n", two - one);

  // Test 1: how long a lookup takes when the item's not found.
  // Generate test data at random, don't store it.
  // Execute lookup based on test data, and time it.
  printf("Lookup test (of random data) on an empty table.\n");
  lookup_test(NULL, NULL);
  printf("\n");

  // Test 2: How long insertion takes
  // FIXME check if item isn't inserted twice? Check for collisions?
  // Generate test data, store in memory so we can later test lookups against it..
  // Execute the insertion based on the test data, and time it.
  table * my_tab = create_table();
  printf("Insertion test (of random data) into an empty table.\n");
  struct test_data * test_dataset = insert_test(my_tab);
  printf("\n");

  // Test 3: How long a lookup takes when the item's expected to be found.
  // Execute lookup again based on test data, and time it.
  printf("Lookup test (of random data) on a table in which the data was previously inserted.\n");
  lookup_test(test_dataset, my_tab);
  printf("\n");

  // Test 4: mix lookups and inserts
  // Execute lookup again based on test data, and time it.
  printf("Random insertion/testing/both of data in an originally-empty table.\n");
  mix_insert_lookup_test();
  printf("\n");

  destroy_table(my_tab);
  free(test_dataset);

  printf("done\n");
  return 0;
}
