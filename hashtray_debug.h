/*
Copyright University of Pennsylvania, 2018

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.


This file: Debugging support for libhashtray.
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
