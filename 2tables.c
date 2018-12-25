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


This file: Example of using 2 tables simultaneously from libhashtray
*/

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "hashtray_M100.h"
#include "hashtray_S1000.h"

int
main() {
  struct HASHTRAY_M100_table * tM = HASHTRAY_M100_create_table();
  struct HASHTRAY_S1000_table * tS = HASHTRAY_S1000_create_table();

  HASHTRAY_M100_data_t dataM = 1;
  HASHTRAY_M100_data_t metadataM = 1;
  enum HASHTRAY_M100_outcome oM;
  oM = HASHTRAY_M100_insert(tM, dataM, metadataM, NULL, NULL);
  assert(HASHTRAY_M100_OK == oM);

  HASHTRAY_S1000_data_t dataS = 2;
  HASHTRAY_S1000_data_t metadataS = 2;
  enum HASHTRAY_S1000_outcome oS;
  oS = HASHTRAY_S1000_insert(tS, dataS, metadataS, NULL, NULL);
  assert(HASHTRAY_S1000_OK == oM);

  // FIXME elaborate this example further

  HASHTRAY_M100_destroy_table(tM);
  HASHTRAY_S1000_destroy_table(tS);
}
