/*
Example of using 2 tables simultaneously from libhashtray
Nik Sultana, University of Pennsylvania, February 2018
*/

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "hashtray_M100.h"
#include "hashtray_S1000.h"

int
main() {
  struct HASHTRAY_M100_table * tM = HASHTRAY_M100_create_table();
  struct HASHTRAY_S1000_table * tS = HASHTRAY_S1000_create_table();

  HASHTRAY_M100_data_t dataM = 1;
  HASHTRAY_M100_data_t metadataM = 1;
  enum HASHTRAY_M100_outcome oM;
  oM = HASHTRAY_M100_insert(tM, dataM, metadataM, NULL, NULL);
  assert(HASHTRAY_M100_OK == oM);

  HASHTRAY_S1000_data_t dataS = 2;
  HASHTRAY_S1000_data_t metadataS = 2;
  enum HASHTRAY_S1000_outcome oS;
  oS = HASHTRAY_S1000_insert(tS, dataS, metadataS, NULL, NULL);
  assert(HASHTRAY_S1000_OK == oM);

  // FIXME elaborate this example further

  HASHTRAY_M100_destroy_table(tM);
  HASHTRAY_S1000_destroy_table(tS);
}
