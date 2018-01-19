/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017
*/

#ifndef PCHAST
#define PCHAST

#include <stdbool.h>
#include <stdint.h>

// NOTE keysize needs to be bigger to occupy more of this table size. Currently
//      KEY_TYPE==uint8_t, which allows us to address 256 cells, each with
//      NUM_CELL_ENTRIES entries (i.e., if NUM_CELL_ENTRIES==4 then we can
//      store around a thousand distinct pieces of info).
//
#define TABLE_SIZE 10000

#define NUM_CELL_ENTRIES 4
#define KEY_TYPE uint8_t
#define VALUE_TYPE uint32_t
#define DATA_TYPE uint32_t
#define CHOICES 2
#define MAX_KICKOUTS 500

struct entry {
  bool clear;
  KEY_TYPE key;
  VALUE_TYPE value;
};

struct cell {
  struct entry entry[NUM_CELL_ENTRIES];
};

typedef struct cell table[TABLE_SIZE];

#define PRNG_SEED 193852039
void init_prng(uint64_t seed);
uint32_t prng(void);

char hash_of_uint32_to_char(uint32_t data);
char hash_of_char_to_char(char data);
KEY_TYPE hash_of_KEY_TYPE(KEY_TYPE data);
KEY_TYPE hash_of_DATA_TYPE(DATA_TYPE data);
KEY_TYPE fingerprint_of_DATA_TYPE(DATA_TYPE data);

struct idxs {
  KEY_TYPE idx[CHOICES];
//  uint32_t idx[CHOICES]; FIXME      
};

KEY_TYPE alt_idx(KEY_TYPE idx, KEY_TYPE fingerprint);
struct idxs idxs_of_DATA_TYPE(DATA_TYPE data, KEY_TYPE * fingerprint);

enum outcome {OK = 0, /*COLLISION, -- we allow collisions, i.e., confusing the metadata of data that happen to have the same fingerprint. This saves time when inserting, since it avoids having to look at all the entries in both buckets. Incidentally, this also means that we allow duplicate fingerprints being stored -- so maybe should revisit this.*/
  NOT_FOUND, GAVE_UP, BLOCKS_FULL,
  END_MARKER};

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

enum outcome insert(table * t, DATA_TYPE data, DATA_TYPE metadata);
enum outcome delete(table * t, DATA_TYPE data);
enum outcome lookup(table * t, DATA_TYPE data, DATA_TYPE * metadata);

table * create_table(void);
void destroy_table(table * t);
#endif // PCHAST
