#if !defined(HASHTRAY_S1000_CONFIG_H)
#define HASHTRAY_S1000_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define HASHTRAY(X) hashtray_s1000_##X

#define TABLE_SIZE 1000
#define NUM_CELL_ENTRIES 4
#define CHOICES 2
#define MAX_KICKOUTS 500

#undef MULTITHREADED

typedef uint16_t HASHTRAY(key_t);
typedef uint32_t HASHTRAY(value_t);
typedef uint32_t HASHTRAY(data_t);

#endif
