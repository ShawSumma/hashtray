/*
Copyright University of Pennsylvania, 2018

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.


This file: Example of using a multi-process instance in libhashtray.
*/

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "hashtray_P100.h"

int
main() {
  struct HASHTRAY_P100_table * tP = HASHTRAY_P100_create_table();

  HASHTRAY_P100_data_t dataP = 0;
  HASHTRAY_P100_data_t metadataP = 0;

  int p = fork();

  if (0 == p) {
    dataP = metadataP = 1;
  } else {
    dataP = metadataP = 2;
  }
  printf("p=%d dataP=%d metadataP=%d\n", p, dataP, metadataP);

  enum HASHTRAY_P100_outcome oP;
  oP = HASHTRAY_P100_insert(tP, dataP, metadataP, NULL, NULL);
  assert(HASHTRAY_P100_OK == oP);

  unsigned sleep_time = 5;
  printf("Sleeping for %d s\n", sleep_time);
  sleep(sleep_time);
  printf("Waking\n");

  if (0 == p) {
    dataP = 2;
  } else {
    dataP = 1;
  }
  oP = HASHTRAY_P100_lookup(tP, dataP, &metadataP, NULL);
  assert(HASHTRAY_P100_OK == oP);
  printf("p=%d dataP=%d metadataP=%d\n", p, dataP, metadataP);

  if (0 == p) {
    assert(2 == metadataP);
  } else {
    assert(1 == metadataP);
  }

  if (0 != p) {
    HASHTRAY_P100_destroy_table(tP);
  }
}
