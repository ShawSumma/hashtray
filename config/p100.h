#if !defined(HASHTRAY_P100_CONFIG_H)
#define HASHTRAY_P100_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define HASHTRAY(X) hashtray_p100_##X

#define TABLE_SIZE 100
#define NUM_CELL_ENTRIES 4
#define CHOICES 2
#define MAX_KICKOUTS 500
#define BACKOFF_SLEEP_MICROSEC 10

#define MULTIPROCESS

typedef uint16_t HASHTRAY(key_t);
typedef uint32_t HASHTRAY(value_t);
typedef uint32_t HASHTRAY(data_t);

#endif
