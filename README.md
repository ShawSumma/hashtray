<p align="center">
  <img src="hashtray.png" alt="Hashtray" />
</p>

A compact hash table library for scalable client classification, based on
[cuckoo hashing](https://en.wikipedia.org/wiki/Cuckoo_hashing). Designed for
remembering client state in network applications such as DoS mitigation, where
the table must be fixed-size, fast, and concurrency-friendly.

## Features

- Constant-time lookup and deletion, amortised constant-time insertion
- Partial-key cuckoo filter variant that stores key-value pairs
- Three concurrency modes: single-threaded, multi-threaded (pthreads), multi-process (POSIX shared memory + semaphores)
- Compile-time configuration: table size, cell count, hash function, key/value types
- Same source compiled against different configs to produce distinct table instances
- Multiple table types usable in the same program via generated headers

## Layout

```txt
src/         library source
config/      per-instance configuration headers (m100.h, s1000.h, p100.h)
test/        test programs
build/       generated objects, headers, and binaries
```

## Building

```sh
make test        # build and run all tests
make tests       # build test binaries only
make all         # build static library (build/libhashtray.a)
make headers     # generate standalone headers (build/hashtray_*.h)
make clean
```

To build with debug logging, collision/eviction tracking, and assertions:

```sh
make DEBUGGING=1 test
```

## Configurations

Each config header defines `HASHTRAY(X)` to a unique prefix, plus table
parameters and types. The three included configs are:

| Config | File | Prefix | Size | Concurrency |
|--------|------|--------|------|-------------|
| M100 | `config/m100.h` | `HASHTRAY_M100_` | 100 cells | multi-threaded |
| S1000 | `config/s1000.h` | `HASHTRAY_S1000_` | 1000 cells | single-threaded |
| P100 | `config/p100.h` | `HASHTRAY_P100_` | 100 cells | multi-process |

## API

```c
table_t *create_table(void);
void destroy_table(table_t *t);

bool insert(table_t *t, data_t data, data_t metadata,
    merge_fn_t *merge_fn, expiry_fn_t *expiry_fn);

bool remove(table_t *t, data_t data);
bool contains(table_t *t, data_t data);

data_t lookup(table_t *t, data_t data, apply_fn_t *apply_fn);

key_array_t keys_of_table(table_t *t);
value_array_t values_of_table(table_t *t);

serialised_t serialise_table(table_t *t);
int32_t deserialise_table(table_t *t, int32_t buffer_len, const char *buffer);
```

All names are prefixed at compile time (e.g. `HASHTRAY_M100_insert`,
`HASHTRAY_M100_table_t`).

## Using multiple tables

For programs that need different table types simultaneously, use the generated
headers (`make headers`). These are preprocessor-expanded copies of the API
with all macros resolved:

```c
#include "hashtray_M100.h"
#include "hashtray_S1000.h"
```

See `test/2tables.c` for a working example.

## Adding a new configuration

1. Create a config header in `config/`, e.g. `config/x500.h`:

```c
#define HASHTRAY(X) HASHTRAY_X500_##X

#define TABLE_SIZE 500
#define NUM_CELL_ENTRIES 4
#define CHOICES 2
#define MAX_KICKOUTS 500

#define MULTITHREADED
#define BACKOFF_SLEEP_MICROSEC 10

typedef uint16_t HASHTRAY(key_t);
typedef uint32_t HASHTRAY(value_t);
typedef uint32_t HASHTRAY(data_t);
```

Choose one of: nothing (single-threaded), `MULTITHREADED`, or `MULTIPROCESS`.

1. Add build rules in the Makefile following the existing pattern: library
   objects, hash objects, the matching lock backend, and optionally a generated
   header and test binary.

## License

MIT — see [LICENSE](LICENSE).
