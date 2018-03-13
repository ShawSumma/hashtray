/*
Example of using 2 tables simultaneously from libgarnish
Nik Sultana, University of Pennsylvania, February 2018
*/

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "garnish_M100.h"
#include "garnish_S1000.h"

int
main() {
  struct GARN_M100_table * tM = GARN_M100_create_table();
  struct GARN_S1000_table * tS = GARN_S1000_create_table();

  GARN_M100_data_t dataM = 1;
  GARN_M100_data_t metadataM = 1;
  enum GARN_M100_outcome oM;
  oM = GARN_M100_insert(tM, dataM, metadataM, NULL, NULL);
  assert(GARN_M100_OK == oM);

  GARN_S1000_data_t dataS = 2;
  GARN_S1000_data_t metadataS = 2;
  enum GARN_S1000_outcome oS;
  oS = GARN_S1000_insert(tS, dataS, metadataS, NULL, NULL);
  assert(GARN_S1000_OK == oM);

  // FIXME elaborate this example further

  GARN_M100_destroy_table(tM);
  GARN_S1000_destroy_table(tS);
}
