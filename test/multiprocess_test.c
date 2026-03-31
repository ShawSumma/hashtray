// Example of using a multi-process instance in libhashtray.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "hashtray_P100.h"

int main(void) {
  hashtray_p100_table_t *table = hashtray_p100_create_table();

  hashtray_p100_data_t data = 0;
  hashtray_p100_data_t metadata = 0;

  int32_t pid = fork();

  if (pid == 0) {
    data = metadata = 1;
  } else {
    data = metadata = 2;
  }
  printf("pid=%d data=%d metadata=%d\n", pid, data, metadata);

  assert(hashtray_p100_insert(table, data, metadata, NULL, NULL));

  uint32_t sleep_time = 5;
  printf("Sleeping for %d s\n", sleep_time);
  sleep(sleep_time);
  printf("Waking\n");

  if (pid == 0) {
    data = 2;
  } else {
    data = 1;
  }
  assert(hashtray_p100_contains(table, data));
  metadata = hashtray_p100_lookup(table, data, NULL);
  printf("pid=%d data=%d metadata=%d\n", pid, data, metadata);

  if (pid == 0) {
    assert(metadata == 2);
  } else {
    assert(metadata == 1);
  }

  if (pid != 0) {
    hashtray_p100_destroy_table(table);
  }
}
