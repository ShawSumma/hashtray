![Hashtray logo](hashtray.png)

# About
**libhashtray** provides an implementation of [cuckoo hashing](https://en.wikipedia.org/wiki/Cuckoo_hashing),
and can provide wrappers to use third-party hash tables using the same
interface.

The latter is useful for applications that want to use one or more of these
hashtable implementations simultaneously.

# Version
1.0

# Downloading
[gitlab](https://www.gitlab.com/niksu/hashtray)

# Building
Running `make headers`
and `make libhashtray.a`
generates the outputs for development and linking.

The included tests and example code is compiled using `make tests`.
Specific tests can be compiled using the appropriate target, and an extensive
debug mode can be used by prepending a flag, e.g., `DEBUGGING=1 make hashtray_multiprocess`.

# Using
See tests for examples.
There is also a [developer documentation](DEVELOPER.md).

# Getting support
Email the author.

# Author
Nik Sultana

# License
[MIT](LICENSE)
