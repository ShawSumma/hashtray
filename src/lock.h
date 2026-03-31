#if !defined(LOCK_H)
#define LOCK_H

#include <stdbool.h>
#include <stdint.h>

#if defined(MULTITHREADED) && defined(MULTIPROCESS)
#error Simultaneous MULTITHREADED and MULTIPROCESS not supported.
#endif

#if defined(MULTITHREADED)

#include <pthread.h>
typedef pthread_mutex_t lock_t;

#elif defined(MULTIPROCESS)

#include <semaphore.h>
typedef sem_t *lock_t;

#else

typedef char lock_t;

#endif

#include "common.h"

lock_t hashtray_lock_init(int32_t index);
void hashtray_lock_destroy(lock_t *l, int32_t index);
void hashtray_lock_acquire(lock_t *l);
void hashtray_lock_release(lock_t *l);
bool hashtray_lock_try(lock_t *l);

#endif
