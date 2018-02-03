/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, February 2018
*/

#include <assert.h>
#include <stdint.h>

#include "pchast_M100.h"
#include "pchast_S1000.h"

int
main() {
  struct PCH_M100_table * tM = PCH_M100_create_table();
  struct PCH_S1000_table * tS = PCH_S1000_create_table();

  PCH_M100_data_t dataM = 1;
  PCH_M100_data_t metadataM = 1;
  enum PCH_M100_outcome oM;
  oM = PCH_M100_insert(tM, dataM, metadataM);
  assert(PCH_M100_OK == oM);

  PCH_S1000_data_t dataS = 2;
  PCH_S1000_data_t metadataS = 2;
  enum PCH_S1000_outcome oS;
  oS = PCH_S1000_insert(tS, dataS, metadataS);
  assert(PCH_S1000_OK == oM);

  // FIXME elaborate this example further

  PCH_M100_destroy_table(tM);
  PCH_S1000_destroy_table(tS);
}
