#if !defined(HASHTRAY)
#error HASHTRAY not defined
#endif

// types
#define hashtray_table_t HASHTRAY(table_t)
#define hashtray_key_t HASHTRAY(key_t)
#define hashtray_value_t HASHTRAY(value_t)
#define hashtray_data_t HASHTRAY(data_t)
#define hashtray_serialised_t HASHTRAY(serialised_t)
#define hashtray_key_array_t HASHTRAY(key_array_t)
#define hashtray_value_array_t HASHTRAY(value_array_t)

// function pointer types
#define hashtray_merge_fn_t HASHTRAY(merge_fn_t)
#define hashtray_expiry_fn_t HASHTRAY(expiry_fn_t)
#define hashtray_apply_fn_t HASHTRAY(apply_fn_t)

// table operations
#define hashtray_create_table HASHTRAY(create_table)
#define hashtray_destroy_table HASHTRAY(destroy_table)
#define hashtray_insert HASHTRAY(insert)
#define hashtray_remove HASHTRAY(remove)
#define hashtray_contains HASHTRAY(contains)
#define hashtray_lookup HASHTRAY(lookup)
#define hashtray_serialise_table HASHTRAY(serialise_table)
#define hashtray_deserialise_table HASHTRAY(deserialise_table)
#define hashtray_keys_of_table HASHTRAY(keys_of_table)
#define hashtray_values_of_table HASHTRAY(values_of_table)
#define hashtray_rand_range HASHTRAY(rand_range)

// hash
#define hashtray_hash_key HASHTRAY(hash_key)
#define hashtray_fingerprint HASHTRAY(fingerprint)

// lock
#define hashtray_lock_init HASHTRAY(lock_init)
#define hashtray_lock_destroy HASHTRAY(lock_destroy)
#define hashtray_lock_acquire HASHTRAY(lock_acquire)
#define hashtray_lock_release HASHTRAY(lock_release)
#define hashtray_lock_try HASHTRAY(lock_try)

// debug (REMEMBER_LOSS)
#if defined(REMEMBER_LOSS)
#define hashtray_overfill_t HASHTRAY(overfill_t)
#define hashtray_overfill HASHTRAY(overfill)
#define hashtray_overfill_idx HASHTRAY(overfill_idx)
#define hashtray_print_overfill HASHTRAY(print_overfill)
#define hashtray_reset_overfill HASHTRAY(reset_overfill)
#define hashtray_has_overflowed HASHTRAY(has_overflowed)
#endif

// debug (REMEMBER_COLLISIONS)
#if defined(REMEMBER_COLLISIONS)
#define hashtray_collision_t HASHTRAY(collision_t)
#define hashtray_collision HASHTRAY(collision)
#define hashtray_collision_idx HASHTRAY(collision_idx)
#define hashtray_print_collision HASHTRAY(print_collision)
#define hashtray_reset_collision HASHTRAY(reset_collision)
#define hashtray_has_collided HASHTRAY(has_collided)
#endif
