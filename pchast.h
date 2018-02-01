/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017
*/

#ifndef PCHAST
#define PCHAST

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef MULTITHREADED
#include <pthread.h>
#endif // MULTITHREADED

// NOTE keysize needs to be bigger to occupy a larger table size. Initially
//      KEY_TYPE==uint8_t, which allows us to address 256 cells, each with
//      NUM_CELL_ENTRIES entries (i.e., if NUM_CELL_ENTRIES==4 then we can
//      store around a thousand distinct pieces of info).
//
#define TABLE_SIZE 100

#define NUM_CELL_ENTRIES 4
#define KEY_TYPE uint16_t
#define VALUE_TYPE uint32_t
#define DATA_TYPE uint32_t
// CHOICES and MAX_KICKOUTS parameters follow the Cuckoo Filter paper.
#define CHOICES 2
#define MAX_KICKOUTS 500

// If two threads try to lock the same cells in the table then at least one of
// them will unlock everything and sleep for a random number of microseconds
// between 0 and BACKOFF_SLEEP_MICROSEC.
#define BACKOFF_SLEEP_MICROSEC 10

struct entry;
struct cell;
struct table;

int rand_range(int min, int max);

enum outcome {OK = 0, NOT_FOUND, GAVE_UP, BLOCKS_FULL, END_MARKER};

struct table * create_table(void);
void destroy_table(struct table * t);

// If "insert" finds a k-v mapping for the same key, then it behaves like "update.
enum outcome insert(struct table * t, DATA_TYPE data, DATA_TYPE metadata);
enum outcome delete(struct table * t, DATA_TYPE data);
enum outcome lookup(struct table * t, DATA_TYPE data, DATA_TYPE * metadata);
enum outcome update(struct table * t, DATA_TYPE data, DATA_TYPE metadata);

#endif // PCHAST
