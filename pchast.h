/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017
*/

#ifndef PCHAST
#define PCHAST

#include <stdbool.h>
#include <stdint.h>

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

struct entry;
struct cell;
struct table;

int rand_range(int min, int max);

KEY_TYPE fingerprint_of_DATA_TYPE(DATA_TYPE data);

struct idxs {
  KEY_TYPE idx[CHOICES];
};

KEY_TYPE alt_idx(KEY_TYPE idx, KEY_TYPE fingerprint);
struct idxs idxs_of_DATA_TYPE(DATA_TYPE data, KEY_TYPE * fingerprint);

enum outcome {OK = 0, /*COLLISION, -- we allow collisions, i.e., confusing the metadata of data that happen to have the same fingerprint. This saves time when inserting, since it avoids having to look at all the entries in both buckets. Incidentally, this also means that we allow duplicate fingerprints being stored -- so maybe should revisit this.*/
  NOT_FOUND, GAVE_UP, BLOCKS_FULL,
  END_MARKER};

struct table * create_table(void);
void destroy_table(struct table * t);

enum outcome insert(struct table * t, DATA_TYPE data, DATA_TYPE metadata);
enum outcome delete(struct table * t, DATA_TYPE data);
enum outcome lookup(struct table * t, DATA_TYPE data, DATA_TYPE * metadata);
enum outcome update(struct table * t, DATA_TYPE data, DATA_TYPE metadata);


extern const char * outcome_str[];
#define PRINT_OUTCOME(o) { \
  printf("%s\n", outcome_str[o]); \
}

typedef unsigned outcome_count[END_MARKER];
#define RESET_OUTCOME_STATS(outcome_count) { \
  for (int i = 0; i < END_MARKER; i++) { \
    outcome_count[i] = 0; \
  } \
}
#define INCREMENT_OUTCOME(outcome_count, o) { \
  outcome_count[o] += 1; \
}
#define PRINT_OUTCOME_STATS(outcome_count) { \
  for (int i = 0; i < END_MARKER; i++) { \
    printf("%s=%d ", outcome_str[i], outcome_count[i]); \
  } \
  printf("\n"); \
}

#define EXTENDED_MEMORY_FACTOR 10

#ifdef REMEMBER_LOSS
#define NUM_OVERFILL_ENTRIES (TABLE_SIZE * EXTENDED_MEMORY_FACTOR)
struct overfill_t;
extern struct overfill_t overfill;
extern int overfill_idx;
void print_overfill(bool);
void reset_overfill(void);
bool has_overflowed(DATA_TYPE data);
#endif // REMEMBER_LOSS

#ifdef REMEMBER_COLLISIONS
#define NUM_COLLIDED_ENTRIES (TABLE_SIZE * EXTENDED_MEMORY_FACTOR)
struct collision_t;
extern struct collision_t collision;
extern int collision_idx;
void print_collision(bool);
void reset_collision(void);
bool has_collided(DATA_TYPE data, VALUE_TYPE queried_metadata);
#endif // REMEMBER_COLLISIONS

#endif // PCHAST
