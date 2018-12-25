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


This file: hashtray API
*/

#ifndef HASHTRAY
#error HASHTRAY not defined
#endif

typedef KEY_TYPE HASHTRAY(key_t);
typedef VALUE_TYPE HASHTRAY(value_t);
typedef DATA_TYPE HASHTRAY(data_t);

int HASHTRAY(rand_range)(int min, int max);
enum HASHTRAY(outcome) {HASHTRAY(OK) = 0, HASHTRAY(NOT_FOUND), HASHTRAY(GAVE_UP), HASHTRAY(BLOCKS_FULL),
  HASHTRAY(END_MARKER)};

struct HASHTRAY(table);

struct HASHTRAY(table) * HASHTRAY(create_table)(void);
void HASHTRAY(destroy_table)(struct HASHTRAY(table) * t);

// If "insert" finds a k-v mapping for the same key, then it behaves like "update.
enum HASHTRAY(outcome) HASHTRAY(insert)(struct HASHTRAY(table) * t, HASHTRAY(data_t) data, HASHTRAY(data_t) metadata,
   int (*merge_fun)(HASHTRAY(data_t) * stored, const HASHTRAY(data_t) * new),
   int (*expiry_fun)(const HASHTRAY(data_t) * metadata));
enum HASHTRAY(outcome) HASHTRAY(delete)(struct HASHTRAY(table) * t, HASHTRAY(data_t) data);
enum HASHTRAY(outcome) HASHTRAY(lookup)(struct HASHTRAY(table) * t, HASHTRAY(data_t) data, HASHTRAY(data_t) * metadata,
    int (*apply_fun)(HASHTRAY(data_t) * metadata));

int HASHTRAY(serialise_table)(struct HASHTRAY(table) * t, char ** buffer);
int HASHTRAY(deserialise_table)(const char * buffer, const int buffer_len, struct HASHTRAY(table) * t);

void HASHTRAY(keys_of_table)(struct HASHTRAY(table) * t, HASHTRAY(key_t) ** result_array, int * result_array_len);
void HASHTRAY(values_of_table)(struct HASHTRAY(table) * t, HASHTRAY(data_t) ** result_array, int * result_array_len);
