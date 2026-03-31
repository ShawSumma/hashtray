// Single-threaded tester for libhashtray.
// Measures kickouts of the partial-key cuckoo hash and performs correctness
// testing using the debug functions.
// NOTE: parts of this code are tightly coupled with amd64 ISA (rdtscp).

#define _GNU_SOURCE
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "hashtray.h"
#include "hashtray_debug.h"

#define TEST_DATASET_SIZE 5000

typedef struct test_data_t test_data_t;

struct test_data_t {
  HASHTRAY(data_t) datum;
  HASHTRAY(value_t) metadatum;
};

#define TARGET_CORE 0
#define COOL_THE_CACHE 0
#define CACHE_COOLING_BLOCK 5000000

int32_t cool_cache(void) {
  int32_t nonse = 0;
  char *buf = malloc(sizeof(*buf) * CACHE_COOLING_BLOCK);
  for (int32_t i = 0; i < CACHE_COOLING_BLOCK; i++) {
    buf[i] += 1;
    nonse += buf[i];
  }
  free(buf);
  return nonse;
}

void simple_test(HASHTRAY(data_t) data, HASHTRAY(data_t) metadata) {
  printf("simple_test: create, query, insert, query, update, query, delete, re-delete, re-query, destroy.\n");
  HASHTRAY(table_t) *my_tab = HASHTRAY(create_table)();

  assert(!HASHTRAY(contains)(my_tab, data));

  assert(HASHTRAY(insert)(my_tab, data, metadata, NULL, NULL));
  assert(HASHTRAY(contains)(my_tab, data));
  assert(HASHTRAY(lookup)(my_tab, data, NULL) == metadata);

  assert(HASHTRAY(insert)(my_tab, data, metadata + 1, NULL, NULL));
  assert(HASHTRAY(contains)(my_tab, data));
  assert(HASHTRAY(lookup)(my_tab, data, NULL) == (metadata + 1));

  assert(HASHTRAY(remove)(my_tab, data));
  assert(!HASHTRAY(remove)(my_tab, data));
  assert(!HASHTRAY(contains)(my_tab, data));

  HASHTRAY(destroy_table)(my_tab);
}

#if defined(__x86_64__)
static inline uint64_t rdtscp(uint32_t *aux) {
  uint64_t rax, rdx;
  __asm volatile("rdtscp\n" : "=a"(rax), "=d"(rdx), "=c"(aux) : :);
  return (rdx << 32) + rax;
}
#else
static inline uint64_t rdtscp(uint32_t *aux) {
  (void)aux;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
#endif

static inline void update_stats(
  int32_t iteration,
  uint64_t time_before,
  uint64_t time_after,
  uint64_t *average,
  uint64_t *max,
  uint64_t *min
) {
  uint64_t x = time_after - time_before;
  if (iteration == 0 || (*average == 0 && *max == 0 && *min == 0)) {
    *average = x;
    *max = x;
    *min = x;
  } else {
    *average = (*average + x) >> 1;
    *max = x > *max ? x : *max;
    *min = x < *min ? x : *min;
  }
}

void lookup_test(test_data_t *test_dataset, HASHTRAY(table_t) *test_table) {
  uint64_t average = 0;
  uint64_t max = 0;
  uint64_t min = 0;

  bool temporary_table = false;
  if (test_table == NULL) {
    test_table = HASHTRAY(create_table)();
    temporary_table = true;
  }

  int32_t found = 0;
  int32_t not_found = 0;

  for (int32_t i = 0; i < TEST_DATASET_SIZE; i++) {
    HASHTRAY(data_t) data;

    if (test_dataset != NULL) {
      data = test_dataset[i].datum;
    } else {
      data = (HASHTRAY(data_t)) HASHTRAY(rand_range)(0, INT_MAX);
    }

    #if COOL_THE_CACHE
    (void) cool_cache();
    #endif

    uint32_t aux;
    uint64_t one = rdtscp(&aux);
    bool hit = HASHTRAY(contains)(test_table, data);
    uint64_t two = rdtscp(&aux);

    if (temporary_table) {
      assert(!hit);
    } else if (hit) {
      HASHTRAY(data_t) value = HASHTRAY(lookup)(test_table, data, NULL);
      if (value != test_dataset[i].metadatum) {
        #if defined(REMEMBER_COLLISIONS)
        assert(HASHTRAY(has_collided)(data, value));
        #endif
      }
      found++;
    } else {
      #if defined(REMEMBER_LOSS)
      assert(HASHTRAY(has_overflowed)(data));
      #endif
      not_found++;
    }
    update_stats(i, one, two, &average, &max, &min);
  }

  if (temporary_table) {
    HASHTRAY(destroy_table)(test_table);
  }

  printf("lookup_test duration (ticks): ");
  printf("min=%" PRIu64 "\t", min);
  printf("avg=%" PRIu64 "\t", average);
  printf("max=%" PRIu64 "\n", max);
  printf("found=%d\t", found);
  printf("not_found=%d\n", not_found);

  #if defined(REMEMBER_LOSS)
  HASHTRAY(print_overfill)(false);
  HASHTRAY(reset_overfill)();
  #endif

  #if defined(REMEMBER_COLLISIONS)
  HASHTRAY(print_collision)(false);
  #endif
}

static test_data_t *generate_test_input(void) {
  test_data_t *result = malloc(sizeof(test_data_t) * TEST_DATASET_SIZE);
  for (int32_t i = 0; i < TEST_DATASET_SIZE; i++) {
    result[i].datum = (HASHTRAY(data_t)) i;
    result[i].metadatum = (HASHTRAY(value_t)) i;
  }
  #if defined(HASHTRAY_LOG_INSERTS)
  for (int32_t i = 0; i < TEST_DATASET_SIZE; i++) {
    printf("Test entry %d: (%d, %d)\n", i, result[i].datum, result[i].metadatum);
  }
  #endif
  return result;
}

test_data_t *insert_test(HASHTRAY(table_t) *test_table) {
  assert(test_table != NULL);

  uint64_t average = 0;
  uint64_t max = 0;
  uint64_t min = 0;

  test_data_t *result = generate_test_input();

  int32_t successes = 0;
  int32_t failures = 0;

  for (int32_t i = 0; i < TEST_DATASET_SIZE; i++) {
    #if COOL_THE_CACHE
    (void) cool_cache();
    #endif

    uint32_t aux;
    uint64_t one = rdtscp(&aux);
    bool ok = HASHTRAY(insert)(test_table, result[i].datum, result[i].metadatum, NULL, NULL);
    uint64_t two = rdtscp(&aux);

    if (ok) {
      successes++;
    } else {
      failures++;
    }
    update_stats(i, one, two, &average, &max, &min);
  }

  printf("insert_test duration (ticks): ");
  printf("min=%" PRIu64 "\t", min);
  printf("avg=%" PRIu64 "\t", average);
  printf("max=%" PRIu64 "\n", max);
  printf("successes=%d\t", successes);
  printf("failures=%d\n", failures);

  #if defined(REMEMBER_LOSS)
  HASHTRAY(print_overfill)(false);
  HASHTRAY(reset_overfill)();
  #endif

  #if defined(REMEMBER_COLLISIONS)
  HASHTRAY(print_collision)(false);
  #endif

  return result;
}

#define STATE_INSERT 0
#define STATE_INSERT_AND_LOOKUP 1
#define STATE_LOOKUP 2

void mix_insert_lookup_test(void) {
  uint64_t average_insert = 0;
  uint64_t max_insert = 0;
  uint64_t min_insert = 0;
  int32_t successes_insert = 0;
  int32_t failures_insert = 0;

  uint64_t average_lookup_expectfind = 0;
  uint64_t max_lookup_expectfind = 0;
  uint64_t min_lookup_expectfind = 0;
  int32_t successes_lookup_expectfind = 0;

  uint64_t average_lookup_notexpectfind = 0;
  uint64_t max_lookup_notexpectfind = 0;
  uint64_t min_lookup_notexpectfind = 0;
  int32_t found_lookup_notexpectfind = 0;
  int32_t not_found_lookup_notexpectfind = 0;

  int32_t state = STATE_INSERT;

  HASHTRAY(table_t) *test_table = HASHTRAY(create_table)();

  for (int32_t i = 0; i < TEST_DATASET_SIZE; i++) {
    switch (HASHTRAY(rand_range)(STATE_INSERT, STATE_LOOKUP)) {
    case 0:
      state = STATE_INSERT;
      break;
    case 1:
      state = STATE_INSERT_AND_LOOKUP;
      break;
    case 2:
      state = STATE_LOOKUP;
      break;
    default:
      assert(false);
    }

    HASHTRAY(data_t) data = (HASHTRAY(data_t)) HASHTRAY(rand_range)(0, INT_MAX);
    HASHTRAY(value_t) metadata = (HASHTRAY(value_t)) HASHTRAY(rand_range)(0, INT_MAX);

    #if COOL_THE_CACHE
    (void) cool_cache();
    #endif

    uint32_t aux;
    uint64_t one;
    uint64_t two;

    switch (state) {
    case STATE_INSERT: {
      one = rdtscp(&aux);
      bool ok = HASHTRAY(insert)(test_table, data, metadata, NULL, NULL);
      two = rdtscp(&aux);
      if (ok) {
        successes_insert++;
      } else {
        failures_insert++;
      }
      update_stats(i, one, two, &average_insert, &max_insert, &min_insert);
      break;
    }

    case STATE_INSERT_AND_LOOKUP:
      HASHTRAY(insert)(test_table, data, metadata, NULL, NULL);
      one = rdtscp(&aux);
      assert(HASHTRAY(contains)(test_table, data));
      two = rdtscp(&aux);
      successes_lookup_expectfind++;
      update_stats(i, one, two, &average_lookup_expectfind, &max_lookup_expectfind, &min_lookup_expectfind);
      break;

    case STATE_LOOKUP: {
      one = rdtscp(&aux);
      bool hit = HASHTRAY(contains)(test_table, data);
      two = rdtscp(&aux);
      if (hit) {
        found_lookup_notexpectfind++;
      } else {
        not_found_lookup_notexpectfind++;
      }
      update_stats(i, one, two, &average_lookup_notexpectfind, &max_lookup_notexpectfind, &min_lookup_notexpectfind);
      break;
    }
    default:
      assert(false);
    }
  }

  HASHTRAY(destroy_table)(test_table);

  printf("mix_insert_lookup_test:INSERT duration (ticks): ");
  printf("min=%" PRIu64 "\t", min_insert);
  printf("avg=%" PRIu64 "\t", average_insert);
  printf("max=%" PRIu64 "\n", max_insert);
  printf("successes=%d\t", successes_insert);
  printf("failures=%d\n", failures_insert);

  printf("mix_insert_lookup_test:INSERT_AND_LOOKUP duration (ticks): ");
  printf("min=%" PRIu64 "\t", min_lookup_expectfind);
  printf("avg=%" PRIu64 "\t", average_lookup_expectfind);
  printf("max=%" PRIu64 "\n", max_lookup_expectfind);
  printf("successes=%d\n", successes_lookup_expectfind);

  printf("mix_insert_lookup_test:LOOKUP duration (ticks): ");
  printf("min=%" PRIu64 "\t", min_lookup_notexpectfind);
  printf("avg=%" PRIu64 "\t", average_lookup_notexpectfind);
  printf("max=%" PRIu64 "\n", max_lookup_notexpectfind);
  printf("found=%d\t", found_lookup_notexpectfind);
  printf("not_found=%d\n", not_found_lookup_notexpectfind);
}

void test_serialisation(HASHTRAY(table_t) *table1, test_data_t *test_dataset) {
  HASHTRAY(serialised_t) s1 = HASHTRAY(serialise_table)(table1);
  assert(s1.size > 0);

  HASHTRAY(table_t) *table2 = HASHTRAY(create_table)();
  int32_t result = HASHTRAY(deserialise_table)(table2, s1.size, s1.buffer);
  assert(result >= 0);

  HASHTRAY(serialised_t) s2 = HASHTRAY(serialise_table)(table2);
  assert(s1.size == s2.size);

  for (int32_t i = 0; i < s2.size; i++) {
    assert(s1.buffer[i] == s2.buffer[i]);
  }

  for (int32_t i = 0; i < TEST_DATASET_SIZE; i++) {
    bool c1 = HASHTRAY(contains)(table1, test_dataset[i].datum);
    bool c2 = HASHTRAY(contains)(table2, test_dataset[i].datum);
    assert(c1 == c2);
    if (c1) {
      assert(HASHTRAY(lookup)(table1, test_dataset[i].datum, NULL) == HASHTRAY(lookup)(table2, test_dataset[i].datum, NULL));
    }
  }

  printf("tested serialisation.\n");
}

void test_extraction(HASHTRAY(table_t) *tbl) {
  HASHTRAY(key_array_t) keys = HASHTRAY(keys_of_table)(tbl);
  HASHTRAY(value_array_t) values = HASHTRAY(values_of_table)(tbl);

  assert(values.len == keys.len);
  printf("extracted kv arrays: they have %d elements\n", keys.len);

  if (keys.data != NULL) {
    free(keys.data);
  }
  if (values.data != NULL) {
    free(values.data);
  }
}

int main(void) {
  srand(1802 * 9373);

  printf("TABLE_SIZE=%d\t", TABLE_SIZE);
  printf("NUM_CELL_ENTRIES=%d\t", NUM_CELL_ENTRIES);
  printf("TEST_DATASET_SIZE=%d\n", TEST_DATASET_SIZE);

  uint32_t aux;
  uint64_t one = rdtscp(&aux);
  simple_test(0, 0);
  uint64_t two = rdtscp(&aux);
  printf("simple_test duration: %" PRIu64 " ticks\n\n", two - one);

  printf("Lookup test (of random data) on an empty table.\n");
  lookup_test(NULL, NULL);
  printf("\n");

  HASHTRAY(table_t) *my_tab = HASHTRAY(create_table)();
  printf("Insertion test (of unique data items) into an empty table.\n");
  test_data_t *test_dataset = insert_test(my_tab);
  printf("\n");

  printf("Lookup test (of previously-generated data) on a table in which the data was previously inserted.\n");
  lookup_test(test_dataset, my_tab);
  printf("\n");

  #if defined(REMEMBER_LOSS)
  HASHTRAY(reset_overfill)();
  #endif

  #if defined(REMEMBER_COLLISIONS)
  HASHTRAY(reset_collision)();
  #endif

  printf("Random insertion/testing/both of data in an originally-empty table.\n");
  mix_insert_lookup_test();
  printf("\n");

  test_serialisation(my_tab, test_dataset);
  test_extraction(my_tab);

  HASHTRAY(destroy_table)(my_tab);
  free(test_dataset);

  printf("done\n");
  return 0;
}
