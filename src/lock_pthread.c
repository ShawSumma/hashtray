#include "lock.h"

#include "hashtray_assert.h"

lock_t hashtray_lock_init(int32_t index) {
  (void) index;
  lock_t l;
  hashtray_assert(!pthread_mutex_init(&l, NULL));
  return l;
}

void hashtray_lock_destroy(lock_t *l, int32_t index) {
  (void) index;
  hashtray_assert(!pthread_mutex_destroy(l));
}

void hashtray_lock_acquire(lock_t *l) {
  hashtray_assert(!pthread_mutex_lock(l));
}

void hashtray_lock_release(lock_t *l) {
  hashtray_assert(!pthread_mutex_unlock(l));
}

bool hashtray_lock_try(lock_t *l) {
  return pthread_mutex_trylock(l) == 0;
}
