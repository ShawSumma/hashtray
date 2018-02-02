/*
Partial-key cuckoo hash.
(aka A cuckoo filter with a value associated with each fingerprint)
Nik Sultana, University of Pennsylvania, November 2017
*/

#ifndef PCHAST
#define PCHAST

struct table;

int rand_range(int min, int max);

enum outcome {OK = 0, NOT_FOUND, GAVE_UP, BLOCKS_FULL, END_MARKER};

struct table * create_table(void);
void destroy_table(struct table * t);

// If "insert" finds a k-v mapping for the same key, then it behaves like "update.
enum outcome insert(struct table * t, DATA_TYPE data, DATA_TYPE metadata);
enum outcome delete(struct table * t, DATA_TYPE data);
enum outcome lookup(struct table * t, DATA_TYPE data, DATA_TYPE * metadata);
enum outcome update(struct table * t, DATA_TYPE data, DATA_TYPE metadata);

#endif // PCHAST
