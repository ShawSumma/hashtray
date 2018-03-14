/*
Single-threaded tester for libgarnish.
Measures the kickouts etc of the partial-key cuckoo hash implementation, and performs correctness testing using the debug functions.
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
#include <limits.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "garnish.h"
#include "garnish_debug.h"

#define TEST_DATASET_SIZE 5000
struct test_data {
  GARN(data_t) datum;
  GARN(value_t) metadatum;
};

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
  free(buf);
  return nonse;
}

void
simple_test(GARN(data_t) data, GARN(data_t) metadata)
{
  printf("simple_test: create table, query for a piece of data, insert data, query for that data, reinsert(update) data, requery for that data, delete that data, re-delete that data, re-query for that data, destroy table.\n");
  struct GARN(table) * my_tab = GARN(create_table)();
  enum GARN(outcome) o;
  GARN(data_t) queried_metadata = 0;
  o = GARN(lookup)(my_tab, data, &queried_metadata, NULL);
  assert(GARN(NOT_FOUND) == o);

  o = GARN(insert)(my_tab, data, metadata, NULL, NULL);
  assert(GARN(OK) == o);

  o = GARN(lookup)(my_tab, data, &queried_metadata, NULL);
  assert(GARN(OK) == o);
  assert(queried_metadata == metadata);

  o = GARN(insert)(my_tab, data, metadata + 1, NULL, NULL);
  assert(GARN(OK) == o);

  o = GARN(lookup)(my_tab, data, &queried_metadata, NULL);
  assert(GARN(OK) == o);
  assert(queried_metadata == (metadata + 1));

  o = GARN(delete)(my_tab, data);
  assert(GARN(OK) == o);

  o = GARN(delete)(my_tab, data);
  assert(GARN(NOT_FOUND) == o);
  o = GARN(lookup)(my_tab, data, &queried_metadata, NULL);
  assert(GARN(NOT_FOUND) == o);

  GARN(destroy_table)(my_tab);
}

// Based on https://stackoverflow.com/questions/14783782/which-inline-assembly-code-is-correct-for-rdtscp#14783909
static inline uint64_t rdtscp(uint32_t * aux)
{
    uint64_t rax,rdx;
    __asm volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
    return (rdx << 32) + rax;
}

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
lookup_test(struct test_data * test_dataset, struct GARN(table) * test_table)
{
  uint64_t average = 0;
  uint64_t max = 0;
  uint64_t min = 0;

  bool temporary_table = false;
  if (NULL == test_table) {
    test_table = GARN(create_table)();
    temporary_table = true;
  }

  enum GARN(outcome) o;

  outcome_count oc;
  RESET_OUTCOME_STATS(oc)

  GARN(data_t) queried_metadata = 0;
  for (int i = 0; i < TEST_DATASET_SIZE; i++) {
    GARN(data_t) data;

    if (NULL != test_dataset) {
      data = test_dataset[i].datum;
    } else {
      data = (GARN(data_t))GARN(rand_range)(0, INT_MAX);
    }

#if COOL_THE_CACHE
    (void)cool_cache();
#endif

    uint32_t aux;
    uint64_t one = rdtscp(&aux);
    o = GARN(lookup)(test_table, data, &queried_metadata, NULL);
    uint64_t two = rdtscp(&aux);

#if 0
    PRINT_OUTCOME(o);
#endif
    if (temporary_table) {
      assert(GARN(NOT_FOUND) == o);
    } else {
      assert(GARN(OK) == o || // Everything in test_dataset should appear in test_table...
             GARN(NOT_FOUND) == o/*..but it might not appear, if it's been kicked
                             out: there's a check for this further down.*/);
      if (GARN(OK) == o) {
        // Check if this fails because of collision.
        if (queried_metadata != test_dataset[i].metadatum) {
#if 0
          GARN(key_t) fingerprint = fingerprint_of_DATA_TYPE(data);
          printf("Unexpected result for data=%d (key=%d): expected %d but retrieved %d\n",
              data, fingerprint, test_dataset[i].metadatum, queried_metadata);
#endif
#ifdef REMEMBER_COLLISIONS
          assert(has_collided(data, queried_metadata));
#if 0
          printf("  Confirmed that this was due to collisions.\n");
#endif
#endif // REMEMBER_COLLISIONS
        }
      } else if (GARN(NOT_FOUND) == o) {
#ifdef REMEMBER_LOSS
        // We expect this item to have been found, so it must have overflowed.
        assert(has_overflowed(data));
#endif // REMEMBER_LOSS
      }
    }
    INCREMENT_OUTCOME(oc, o)
    update_stats(i, one, two, &average, &max, &min);
  }

  if (temporary_table) {
    GARN(destroy_table)(test_table);
  }

  printf("lookup_test min / average / max duration: %llu / %llu / %llu ticks\n",
      min, average, max);
  PRINT_OUTCOME_STATS(oc)

#ifdef REMEMBER_LOSS
  print_overfill(false);
  reset_overfill();
#endif // REMEMBER_LOSS
#ifdef REMEMBER_COLLISIONS
  print_collision(false);
#endif // REMEMBER_COLLISIONS
}

static struct test_data *
generate_test_input(void)
{
  struct test_data * result = malloc(sizeof(struct test_data) * TEST_DATASET_SIZE);
  for (int i = 0; i < TEST_DATASET_SIZE; i++) {
    result[i].datum = (GARN(data_t))i;
    result[i].metadatum = (GARN(value_t))i;
  }
#ifdef LOG_INSERTS
  for (int i = 0; i < TEST_DATASET_SIZE; i++) {
    printf("Test entry %d: (%d, %d)\n", i, result[i].datum, result[i].metadatum);
  }
#endif // LOG_INSERTS
  return result;
}

struct test_data *
insert_test(struct GARN(table) * test_table)
{
  assert(NULL != test_table);

  uint64_t average = 0;
  uint64_t max = 0;
  uint64_t min = 0;

  struct test_data * result = generate_test_input();

  enum GARN(outcome) o;

  outcome_count oc;
  RESET_OUTCOME_STATS(oc)

  for (int i = 0; i < TEST_DATASET_SIZE; i++) {
#if COOL_THE_CACHE
    (void)cool_cache();
#endif

    uint32_t aux;
    uint64_t one = rdtscp(&aux);
    o = GARN(insert)(test_table, result[i].datum, result[i].metadatum, NULL, NULL);
    uint64_t two = rdtscp(&aux);
#if 0
    PRINT_OUTCOME(o);
#endif
    assert(GARN(OK) == o || // Assuming that table doesn't fill up...
           GARN(GAVE_UP) == o/*... otherwise we might give up trying to add an item
                         (after MAX_KICKOUTS has been exceeded).*/);

    INCREMENT_OUTCOME(oc, o)
    update_stats(i, one, two, &average, &max, &min);
  }

  printf("insert_test min / average / max duration: %llu / %llu / %llu ticks\n",
      min, average, max);
  PRINT_OUTCOME_STATS(oc)

#ifdef REMEMBER_LOSS
  print_overfill(false);
  reset_overfill();
#endif // REMEMBER_LOSS

#ifdef REMEMBER_COLLISIONS
  print_collision(false);
  // NOTE We should not reset collision, since this would invalidate
  //      a future test for collisions.
#endif // REMEMBER_COLLISIONS

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

  struct GARN(table) * test_table = GARN(create_table)();
  enum GARN(outcome) o;

  for (int i = 0; i < TEST_DATASET_SIZE; i++) {
    switch (GARN(rand_range)(INSERT, LOOKUP)) {
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

    GARN(data_t) data = (GARN(data_t))GARN(rand_range)(0, INT_MAX);
    GARN(value_t) queried_metadata = (GARN(value_t))GARN(rand_range)(0, INT_MAX);

#if COOL_THE_CACHE
    (void)cool_cache();
#endif

    uint32_t aux;
    uint64_t one;
    uint64_t two;

    switch (state) {
    case INSERT:
      one = rdtscp(&aux);
      o = GARN(insert)(test_table, data, queried_metadata, NULL, NULL);
      two = rdtscp(&aux);
#if 0
      PRINT_OUTCOME(o);
#endif
      assert(GARN(GAVE_UP) == o ||
             GARN(OK) == o);
      INCREMENT_OUTCOME(oc_insert, o)
      update_stats(i, one, two, &average_insert, &max_insert, &min_insert);
      break;

    case INSERT_AND_LOOKUP:
      (void)GARN(insert)(test_table, data, queried_metadata, NULL, NULL);
      one = rdtscp(&aux);
      o = GARN(lookup)(test_table, data, &queried_metadata, NULL);
      two = rdtscp(&aux);
#if 0
      PRINT_OUTCOME(o);
#endif
      assert(GARN(OK) == o);
      INCREMENT_OUTCOME(oc_lookup_expectfind, o)
      update_stats(i, one, two, &average_lookup_expectfind, &max_lookup_expectfind, &min_lookup_expectfind);
      break;

    case LOOKUP:
      one = rdtscp(&aux);
      o = GARN(lookup)(test_table, data, &queried_metadata, NULL);
      two = rdtscp(&aux);
#if 0
      PRINT_OUTCOME(o);
#endif
      assert(GARN(NOT_FOUND) == o ||
             GARN(OK) == o /*This can occur if:
                       * we happen to regenerate a random value that was previously generated during the INSERT phase
                       * in case of a false-positive (unless we started with an empty table and didn't insert anything).*/);
      INCREMENT_OUTCOME(oc_lookup_notexpectfind, o)
      update_stats(i, one, two, &average_lookup_notexpectfind, &max_lookup_notexpectfind, &min_lookup_notexpectfind);
      break;
    }
  }

  GARN(destroy_table)(test_table);

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
  srand(1802 * 9373);

  printf("TABLE_SIZE=%d, NUM_CELL_ENTRIES=%d, TEST_DATASET_SIZE=%d\n",
      TABLE_SIZE, NUM_CELL_ENTRIES, TEST_DATASET_SIZE);

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
  // (We can set a compilation directive for garnish to record hash collisions)
  // (We generate the data by enumeration, not randomisation, so we shouldn't try to insert the same item twice.)
  // Generate test data, store in memory so we can later test lookups against it..
  // Execute the insertion based on the test data, and time it.
  struct GARN(table) * my_tab = GARN(create_table)();
  printf("Insertion test (of unique data items) into an empty table.\n");
  struct test_data * test_dataset = insert_test(my_tab);
  printf("\n");

  // Test 3: How long a lookup takes when the item's expected to be found.
  // Execute lookup again based on test data, and time it.
  printf("Lookup test (of previously-generated data) on a table in which the data was previously inserted.\n");
  lookup_test(test_dataset, my_tab);
  printf("\n");

  // We can clear these out before the next test.
#ifdef REMEMBER_LOSS
  reset_overfill();
#endif // REMEMBER_LOSS
#ifdef REMEMBER_COLLISIONS
  reset_collision();
#endif // REMEMBER_COLLISIONS

  // Test 4: mix lookups and inserts
  // Execute lookup again based on test data, and time it.
  printf("Random insertion/testing/both of data in an originally-empty table.\n");
  mix_insert_lookup_test();
  printf("\n");

  GARN(destroy_table)(my_tab);
  free(test_dataset);

  printf("done\n");
  return 0;
}
