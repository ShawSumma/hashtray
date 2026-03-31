#include "lock.h"

lock_t hashtray_lock_init(int32_t index) {
  (void) index;
  lock_t l = 0;
  return l;
}

void hashtray_lock_destroy(lock_t *l, int32_t index) {
  (void) l;
  (void) index;
}

void hashtray_lock_acquire(lock_t *l) {
  (void) l;
}

void hashtray_lock_release(lock_t *l) {
  (void) l;
}

bool hashtray_lock_try(lock_t *l) {
  (void) l;
  return true;
}
