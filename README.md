# About
**libgarnish** provides an implementation of cuckoo hashing, and can provide
wrappers to use third-party hash tables using the same interface.

The latter is useful for applications that want to use one or more of these
hashtable implementations simultaneously.

# Downloading
(Hosting to be decided)

# Building
Running `make headers`
and `make libgarnish.a`
generates the outputs for development and linking.

The included tests and example code is compiled using `make tests`.
Specific tests can be compiled using the appropriate target, and an extensive
debug mode can be used by prepending a flag, e.g., `DEBUGGING=1 make garnish_multiprocess`.

# Using
See tests for examples.

# Getting support
Email the author.

# Author
Nik Sultana

# License
(To be decided)
