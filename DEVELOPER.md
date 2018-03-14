# Design
Garnish is designed around a single API ([garnish.h]()) that is configured
with different parameters (e.g., [garnish_M100_config.h]()) to generate child
APIs for use when compiling and linking.
These APIs can be used simultaneously within the same application.

# Root API
The "root" API is given in [garnish.h]() and consists of the basic things
one can do with garnish table, such as initialise it and perform lookups.

# Example derived APIs
The root API only provides an abstract interface into the functionality of a
table: for example, it does not place any constraints over when or how this
interface can be used. Such constraints are properties of an *instance* of
the API. For example, [garnish_M100_config.h]() is used for a form of the API
that is specialised to support a table of 100 cells and is thread-safe,
whereas [garnish_S1000_config.h]() supports 1000 cells and isn't thread-safe.

# Instances
(TODO)

# Adding hashing function implementations
(TODO)

# Wrapping existing table implementations
(TODO)
