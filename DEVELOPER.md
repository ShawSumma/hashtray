# Design
Hashtray is designed around a single API ([hashtray.h]()) that is configured
with different parameters (e.g., [hashtray_M100_config.h]()) to derive child
APIs for use when compiling and linking.
These APIs can be used simultaneously within the same application.

# Root API
The "root" API is given in [hashtray.h]() and consists of the basic things
one can do with hashtray table, such as initialise it and perform lookups.

# Derived APIs
The root API only provides an abstract interface into the functionality of a
table: for example, it does not place any constraints over when or how this
interface can be used.

Such constraints are properties of *derivations* of the API. For example,
[hashtray_M100_config.h]() is used to derive `hashtray_M100.h`, that is the API
specialised to support a table of 100 cells and is thread-safe, whereas
[hashtray_S1000_config.h]() is used to derive `hashtray_S1000.h`, that supports
1000 cells and isn't thread-safe.

# Instances
An instance implements a derived API.
For example, `hashtray_M100.o` implements `hashtray_M100_config.h`.
Adding an instance consists of implementing the hashtray API -- for example
three instances are implemented in [hashtray.c]().

[2tables.c]() shows how to simultaneously use different Hashtray instances.
Applications might need to simultaneously use different instances for different
purposes; we came across this need in other work.

# Adding hashing function implementations
For an example we show how to replace the (keyed) hash function used in hashtray
with [SipHash](https://131002.net/siphash/). The code is in [sip_hash.c]()
and can be built using `make hashtray_multithreaded_siphash`.

Before running that command make sure to have cloned the [SipHash
repo](https://github.com/veorq/SipHash) and run `gcc -c halfsiphash.c` in that
directory.
Then update the `SIPHASH` variable in [Makefile.targets]() to the repo's path.
By default I set `SIPHASH=SipHash` since I clone SipHash within hashtray's directory.

# Wrapping existing table implementations (to instantiate derivated APIs)
Applications can use libhashtray's API when using existing/external hashtable implementations.
This makes it easy to swap hashtable implementations for experimentation.

An example of this approach is given using [uthash](https://github.com/troydhanson/uthash).
The wrapping of this library is given in [hashtray_uthash.c]().
To try it out, run `make hashtray_M100_uthash.o`.
To see an example of this in use in one of the tests, run `make hashtray_multithreaded_uthash`.

To set things up, clone the [uthash](https://github.com/troydhanson/uthash) repo.
and update the `UTHASH` variable in [Makefile.targets]() to the repo's path.
By default I set `UTHASH=uthash` since I clone uthash within hashtray's directory.
