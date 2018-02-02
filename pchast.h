/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017
*/

#ifndef PCHAST
#define PCHAST

#ifndef PCH_PREFIX
#define PCH(X) PCH_ ## X
#else
#define PCH(X) PCH_ ## PCH_PREFIX ## _ ## X
#endif

int PCH(rand_range)(int min, int max);
enum outcome {OK = 0, NOT_FOUND, GAVE_UP, BLOCKS_FULL, END_MARKER};

struct PCH(table);

struct PCH(table) * PCH(create_table)(void);
void PCH(destroy_table)(struct PCH(table) * t);

// If "insert" finds a k-v mapping for the same key, then it behaves like "update.
enum outcome PCH(insert)(struct PCH(table) * t, DATA_TYPE data, DATA_TYPE metadata);
enum outcome PCH(delete)(struct PCH(table) * t, DATA_TYPE data);
enum outcome PCH(lookup)(struct PCH(table) * t, DATA_TYPE data, DATA_TYPE * metadata);
enum outcome PCH(update)(struct PCH(table) * t, DATA_TYPE data, DATA_TYPE metadata);

#endif // PCHAST
