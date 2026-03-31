#include "lock.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>

#include "hashtray_assert.h"

static void semaphore_name(size_t bufsize, char *buf, int32_t index) {
  snprintf(buf, bufsize, "/HASHTRAY_sem_%d", index);
}

lock_t hashtray_lock_init(int32_t index) {
  char name[64];
  semaphore_name(sizeof(name), name, index);
  lock_t l = sem_open(name, O_CREAT, 0600, 1);
  hashtray_assert(l != SEM_FAILED);
  return l;
}

void hashtray_lock_destroy(lock_t *l, int32_t index) {
  hashtray_assert(!sem_close(*l));
  char name[64];
  semaphore_name(sizeof(name), name, index);
  hashtray_assert(!sem_unlink(name));
}

void hashtray_lock_acquire(lock_t *l) {
  hashtray_assert(!sem_wait(*l));
}

void hashtray_lock_release(lock_t *l) {
  hashtray_assert(!sem_post(*l));
}

bool hashtray_lock_try(lock_t *l) {
  return sem_trywait(*l) == 0;
}
