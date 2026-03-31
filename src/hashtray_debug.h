#include "common.h"

#define EXTENDED_MEMORY_FACTOR 10

#if defined(REMEMBER_LOSS)
#define NUM_OVERFILL_ENTRIES (TABLE_SIZE * EXTENDED_MEMORY_FACTOR)
typedef struct hashtray_overfill_t hashtray_overfill_t;
extern hashtray_overfill_t hashtray_overfill;
extern int32_t hashtray_overfill_idx;
void hashtray_print_overfill(bool show_entries);
void hashtray_reset_overfill(void);
bool hashtray_has_overflowed(hashtray_data_t data);
#endif

#if defined(REMEMBER_COLLISIONS)
#define NUM_COLLIDED_ENTRIES (TABLE_SIZE * EXTENDED_MEMORY_FACTOR)
typedef struct hashtray_collision_t hashtray_collision_t;
extern hashtray_collision_t hashtray_collision;
extern int32_t hashtray_collision_idx;
void hashtray_print_collision(bool show_entries);
void hashtray_reset_collision(void);
bool hashtray_has_collided(hashtray_data_t data, hashtray_value_t queried_metadata);
#endif
