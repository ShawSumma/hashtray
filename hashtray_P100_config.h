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
*/

// NOTE keysize needs to be bigger to occupy a larger table size. Initially
//      KEY_TYPE==uint8_t, which allows us to address 256 cells, each with
//      NUM_CELL_ENTRIES entries (i.e., if NUM_CELL_ENTRIES==4 then we can
//      store around a thousand distinct pieces of info).
//
#define TABLE_SIZE 100
// The following parameters follow the Cuckoo Filter paper.
#define NUM_CELL_ENTRIES 4
#define CHOICES 2
#define MAX_KICKOUTS 500

#define KEY_TYPE uint16_t
#define VALUE_TYPE uint32_t
#define DATA_TYPE uint32_t

// If two threads try to lock the same cells in the table then at least one of
// them will unlock everything and sleep for a random number of microseconds
// between 0 and BACKOFF_SLEEP_MICROSEC.
#define BACKOFF_SLEEP_MICROSEC 10

#define MULTIPROCESS

#define HASHTRAY(X) HASHTRAY_ ## P100 ## _ ## X
