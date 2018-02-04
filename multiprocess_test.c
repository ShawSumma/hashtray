/*
Example of using the multi-process instance of the partial-key cuckoo hash.
Nik Sultana, University of Pennsylvania, February 2018
*/

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "pchast_P100.h"

int
main() {
  struct PCH_P100_table * tP = PCH_P100_create_table();

  PCH_P100_data_t dataP = 0;
  PCH_P100_data_t metadataP = 0;

  int p = fork();

  if (0 == p) {
    dataP = metadataP = 1;
  } else {
    dataP = metadataP = 2;
  }
  printf("p=%d dataP=%d metadataP=%d\n", p, dataP, metadataP);

  enum PCH_P100_outcome oP;
  oP = PCH_P100_insert(tP, dataP, metadataP, NULL);
  assert(PCH_P100_OK == oP);

  sleep(5);

  if (0 == p) {
    dataP = 2;
  } else {
    dataP = 1;
  }
  oP = PCH_P100_lookup(tP, dataP, &metadataP);
  assert(PCH_P100_OK == oP);
  printf("p=%d dataP=%d metadataP=%d\n", p, dataP, metadataP);

  if (0 == p) {
    assert(2 == metadataP);
  } else {
    assert(1 == metadataP);
  }

  PCH_P100_destroy_table(tP);
}
