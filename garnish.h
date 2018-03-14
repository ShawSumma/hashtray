/*
garnish API
Nik Sultana, University of Pennsylvania, November 2017
*/

#ifndef GARN
#error GARN not defined
#endif

typedef KEY_TYPE GARN(key_t);
typedef VALUE_TYPE GARN(value_t);
typedef DATA_TYPE GARN(data_t);

int GARN(rand_range)(int min, int max);
enum GARN(outcome) {GARN(OK) = 0, GARN(NOT_FOUND), GARN(GAVE_UP), GARN(BLOCKS_FULL),
  GARN(END_MARKER)};

struct GARN(table);

struct GARN(table) * GARN(create_table)(void);
void GARN(destroy_table)(struct GARN(table) * t);

// If "insert" finds a k-v mapping for the same key, then it behaves like "update.
enum GARN(outcome) GARN(insert)(struct GARN(table) * t, GARN(data_t) data, GARN(data_t) metadata,
   int (*merge_fun)(GARN(data_t) * stored, const GARN(data_t) * new),
   int (*expiry_fun)(const GARN(data_t) * metadata));
enum GARN(outcome) GARN(delete)(struct GARN(table) * t, GARN(data_t) data);
enum GARN(outcome) GARN(lookup)(struct GARN(table) * t, GARN(data_t) data, GARN(data_t) * metadata,
    int (*apply_fun)(GARN(data_t) * metadata));

int GARN(serialise_table)(struct GARN(table) * t, char ** buffer);
int GARN(deserialise_table)(const char * buffer, const int buffer_len, struct GARN(table) * t);
