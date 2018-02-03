/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017
*/

#include <stdint.h>

// NOTE keysize needs to be bigger to occupy a larger table size. Initially
//      KEY_TYPE==uint8_t, which allows us to address 256 cells, each with
//      NUM_CELL_ENTRIES entries (i.e., if NUM_CELL_ENTRIES==4 then we can
//      store around a thousand distinct pieces of info).
//
#define TABLE_SIZE 100
// The following parameters follow the Cuckoo Filter paper.
#define NUM_CELL_ENTRIES 4
#define CHOICES 2
#define MAX_KICKOUTS 500

#define KEY_TYPE uint16_t
#define VALUE_TYPE uint32_t
#define DATA_TYPE uint32_t

// If two threads try to lock the same cells in the table then at least one of
// them will unlock everything and sleep for a random number of microseconds
// between 0 and BACKOFF_SLEEP_MICROSEC.
#define BACKOFF_SLEEP_MICROSEC 10

#define MULTITHREADED

#define PCH(X) PCH_ ## M100 ## _ ## X
