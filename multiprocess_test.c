/*
Example of using a multi-process instance in libgarnish.
Nik Sultana, University of Pennsylvania, February 2018
*/

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "garnish_P100.h"

int
main() {
  struct GARN_P100_table * tP = GARN_P100_create_table();

  GARN_P100_data_t dataP = 0;
  GARN_P100_data_t metadataP = 0;

  int p = fork();

  if (0 == p) {
    dataP = metadataP = 1;
  } else {
    dataP = metadataP = 2;
  }
  printf("p=%d dataP=%d metadataP=%d\n", p, dataP, metadataP);

  enum GARN_P100_outcome oP;
  oP = GARN_P100_insert(tP, dataP, metadataP, NULL, NULL);
  assert(GARN_P100_OK == oP);

  unsigned sleep_time = 5;
  printf("Sleeping for %d s\n", sleep_time);
  sleep(sleep_time);
  printf("Waking\n");

  if (0 == p) {
    dataP = 2;
  } else {
    dataP = 1;
  }
  oP = GARN_P100_lookup(tP, dataP, &metadataP, NULL);
  assert(GARN_P100_OK == oP);
  printf("p=%d dataP=%d metadataP=%d\n", p, dataP, metadataP);

  if (0 == p) {
    assert(2 == metadataP);
  } else {
    assert(1 == metadataP);
  }

  if (0 != p) {
    GARN_P100_destroy_table(tP);
  }
}
