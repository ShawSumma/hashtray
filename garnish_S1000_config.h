/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017
*/

#define TABLE_SIZE 1000
// The following parameters follow the Cuckoo Filter paper.
#define NUM_CELL_ENTRIES 4
#define CHOICES 2
#define MAX_KICKOUTS 500

#define KEY_TYPE uint16_t
#define VALUE_TYPE uint32_t
#define DATA_TYPE uint32_t

#undef MULTITHREADED

#define PCH(X) PCH_ ## S1000 ## _ ## X
