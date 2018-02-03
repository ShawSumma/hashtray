/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, February 2018
*/

#include <stdint.h>

#include "pchast_M100.h"
#include "pchast_S1000.h"

int
main() {
  struct PCH_M100_table * tM = PCH_M100_create_table();
  struct PCH_S1000_table * tS = PCH_S1000_create_table();

  PCH_M100_destroy_table(tM);
  PCH_S1000_destroy_table(tS);
}
