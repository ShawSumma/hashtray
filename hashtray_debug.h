/*
Debugging support for libgarnish.
Nik Sultana, University of Pennsylvania, November 2017
*/

#ifndef HASHTRAY_DEBUG
#define HASHTRAY_DEBUG

extern const char * HASHTRAY(outcome_str)[];
#define PRINT_OUTCOME(o) { \
  printf("%s\n", HASHTRAY(outcome_str)[o]); \
}

typedef unsigned outcome_count[HASHTRAY(END_MARKER)];
#define RESET_OUTCOME_STATS(outcome_count) { \
  for (int i = 0; i < HASHTRAY(END_MARKER); i++) { \
    outcome_count[i] = 0; \
  } \
}
#define INCREMENT_OUTCOME(outcome_count, o) { \
  outcome_count[o] += 1; \
}
#define PRINT_OUTCOME_STATS(outcome_count) { \
  for (int i = 0; i < HASHTRAY(END_MARKER); i++) { \
    printf("%s=%d ", HASHTRAY(outcome_str)[i], outcome_count[i]); \
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

#endif // HASHTRAY_DEBUG
