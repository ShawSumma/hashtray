/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017
*/

#include <stdint.h>

#define TABLE_SIZE 1000

#define NUM_CELL_ENTRIES 4
#define KEY_TYPE uint16_t
#define VALUE_TYPE uint32_t
#define DATA_TYPE uint32_t
// CHOICES and MAX_KICKOUTS parameters follow the Cuckoo Filter paper.
#define CHOICES 2
#define MAX_KICKOUTS 500

#undef MULTITHREADED
