# Design
Garnish is designed around a single API ([garnish.h]()) that is configured
with different parameters (e.g., [garnish_M100_config.h]()) to derive child
APIs for use when compiling and linking.
These APIs can be used simultaneously within the same application.

# Root API
The "root" API is given in [garnish.h]() and consists of the basic things
one can do with garnish table, such as initialise it and perform lookups.

# Derived APIs
The root API only provides an abstract interface into the functionality of a
table: for example, it does not place any constraints over when or how this
interface can be used.

Such constraints are properties of *derivations* of the API. For example,
[garnish_M100_config.h]() is used for a form of the API that is specialised to
support a table of 100 cells and is thread-safe, whereas
[garnish_S1000_config.h]() supports 1000 cells and isn't thread-safe.

# Instances
(TODO)

# Adding hashing function implementations
(TODO)

# Wrapping existing table implementations (to instantiate derivated APIs)
Applications can use libgarnish's API when using existing/external hashtable implementations.
This makes it easy to swap hashtable implementations for experimentation.

An example of this approach is given using [uthash](https://github.com/troydhanson/uthash).
The wrapping of this library is given in [garnish_uthash.c]().
To try it out, run `make garnish_M100_uthash.o`.
To see an example of this in use in one of the tests, run `make garnish_multithreaded_uthash`.

To set things up, clone the [uthash](https://github.com/troydhanson/uthash) repo.
and update the `UTHASH` variable in [Makefile]() to the repo's path.
By default I set `UTHASH=uthash` since I clone uthash within garnish's directory.
