/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017
*/

#ifndef PCH
#error PCH not defined
#endif

typedef KEY_TYPE PCH(key_t);
typedef VALUE_TYPE PCH(value_t);
typedef DATA_TYPE PCH(data_t);

int PCH(rand_range)(int min, int max);
enum PCH(outcome) {PCH(OK) = 0, PCH(NOT_FOUND), PCH(GAVE_UP), PCH(BLOCKS_FULL),
  PCH(END_MARKER)};

struct PCH(table);

struct PCH(table) * PCH(create_table)(void);
void PCH(destroy_table)(struct PCH(table) * t);

// If "insert" finds a k-v mapping for the same key, then it behaves like "update.
enum PCH(outcome) PCH(insert)(struct PCH(table) * t, PCH(data_t) data, PCH(data_t) metadata,
   void (*join_fun)(PCH(data_t) * stored, const PCH(data_t) * new),
   int (*expiry_fun)(const PCH(data_t) * metadata));
enum PCH(outcome) PCH(delete)(struct PCH(table) * t, PCH(data_t) data);
enum PCH(outcome) PCH(lookup)(struct PCH(table) * t, PCH(data_t) data, PCH(data_t) * metadata);
enum PCH(outcome) PCH(update)(struct PCH(table) * t, PCH(data_t) data, PCH(data_t) metadata);
