#if !defined(HASHTRAY_ASSERT_H)
#define HASHTRAY_ASSERT_H

#if defined(HASHTRAY_ASSERT)
#include <assert.h>
#define hashtray_assert(cond) assert(cond)
#else
#define hashtray_assert(cond) ((void) (cond))
#endif

#endif
