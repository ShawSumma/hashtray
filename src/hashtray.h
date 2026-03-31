#if !defined(HASHTRAY)
#error HASHTRAY not defined
#endif

#include "common.h"

typedef struct hashtray_table_t hashtray_table_t;

typedef struct hashtray_serialised_t {
  char *buffer;
  int32_t size;
} hashtray_serialised_t;

typedef struct hashtray_key_array_t {
  hashtray_key_t *data;
  int32_t len;
} hashtray_key_array_t;

typedef struct hashtray_value_array_t {
  hashtray_data_t *data;
  int32_t len;
} hashtray_value_array_t;

typedef int32_t hashtray_merge_fn_t(hashtray_data_t *stored, const hashtray_data_t *incoming);
typedef int32_t hashtray_expiry_fn_t(const hashtray_data_t *metadata);
typedef int32_t hashtray_apply_fn_t(hashtray_data_t *metadata);

int32_t hashtray_rand_range(int32_t min, int32_t max);

hashtray_table_t *hashtray_create_table(void);
void hashtray_destroy_table(hashtray_table_t *t);

bool hashtray_insert(
  hashtray_table_t *t,
  hashtray_data_t data,
  hashtray_data_t metadata,
  hashtray_merge_fn_t *merge_fn,
  hashtray_expiry_fn_t *expiry_fn
);

bool hashtray_remove(hashtray_table_t *t, hashtray_data_t data);

bool hashtray_contains(hashtray_table_t *t, hashtray_data_t data);

hashtray_data_t hashtray_lookup(
  hashtray_table_t *t,
  hashtray_data_t data,
  hashtray_apply_fn_t *apply_fn
);

hashtray_serialised_t hashtray_serialise_table(hashtray_table_t *t);

int32_t hashtray_deserialise_table(
  hashtray_table_t *t,
  int32_t buffer_len,
  const char *buffer
);

hashtray_key_array_t hashtray_keys_of_table(hashtray_table_t *t);

hashtray_value_array_t hashtray_values_of_table(hashtray_table_t *t);
