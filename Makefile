# Partial-key cuckoo hash.
# (aka A cuckoo filter with a value associated with each fingerprint)
# Nik Sultana, University of Pennsylvania, November 2017

# from https://stackoverflow.com/questions/154630/recommended-gcc-warning-options-for-c
CFLAGS+=" -Wall -Wextra -Wformat=2 -Wswitch-default -Wcast-align -Wpointer-arith \
    -Wbad-function-cast -Wstrict-prototypes -Winline -Wundef -Wnested-externs \
    -Wcast-qual -Wshadow -Wwrite-strings -Wconversion -Wunreachable-code \
    -Wstrict-aliasing=2 -fno-common -fstrict-aliasing \
    -std=c99 -pedantic \
    -O3 \
    -DMULTITHREADED -lpthread"

libpchast.a: pchast.o
	ar rcs libpchast.a pchast.o

pchast.o: pchast.c
	$CC -c $CFLAGS pchast.c

.phony clean

clean:
	rm pchast.o libpchast.a
