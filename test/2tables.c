// Example of using 2 tables simultaneously from libhashtray.

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "hashtray_M100.h"
#include "hashtray_S1000.h"

int main(void) {
  hashtray_m100_table_t *m100_table = hashtray_m100_create_table();
  hashtray_s1000_table_t *s1000_table = hashtray_s1000_create_table();

  assert(hashtray_m100_insert(m100_table, 1, 1, NULL, NULL));
  assert(hashtray_s1000_insert(s1000_table, 2, 2, NULL, NULL));

  hashtray_m100_destroy_table(m100_table);
  hashtray_s1000_destroy_table(s1000_table);
}
