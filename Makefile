# Partial-key cuckoo hash.
# (aka A cuckoo filter with a value associated with each fingerprint)
# Nik Sultana, University of Pennsylvania, November 2017

ifdef DEBUGGING
DEBUGGING=-O0 -g -DREMEMBER_LOSS -DREMEMBER_COLLISIONS -DPCHAST_ASSERT \
    -DLOG_INSERTS -DDESCRIBE_COLLISIONS -include stdbool.h
else
DEBUGGING=-O3
endif

# from https://stackoverflow.com/questions/154630/recommended-gcc-warning-options-for-c
CFLAGS+=-Wall -Wextra -Wformat=2 -Wswitch-default -Wcast-align -Wpointer-arith \
    -Wbad-function-cast -Wstrict-prototypes -Winline -Wundef -Wnested-externs \
    -Wcast-qual -Wshadow -Wwrite-strings -Wconversion -Wunreachable-code \
    -Wstrict-aliasing=2 -fno-common -fstrict-aliasing \
    -std=c99 -pedantic \
    $(DEBUGGING)

.PHONY: clean tests dist headers

dist: main.c multithreaded_test.c pchast.c pchast.h pchast_M100_config.h pchast_S1000_config.h Makefile 2tables.c pchast_debug.h pchast_P100_config.h multiprocess_test.c README.md
	tar czvf pchast.tgz $^

libpchast.a: pchast_M100.o pchast_S1000.o pchast_P100.o
	ar rcs $@ $^

headers: pchast_M100.h pchast_S1000.h pchast_P100.h

pchast_M100.h : pchast.h pchast_M100_config.h
	$(CC) -include pchast_M100_config.h $(CFLAGS) -E pchast.h -o $@

pchast_S1000.h : pchast.h pchast_S1000_config.h
	$(CC) -include pchast_S1000_config.h $(CFLAGS) -E pchast.h -o $@

pchast_P100.h : pchast.h pchast_P100_config.h
	$(CC) -include pchast_P100_config.h $(CFLAGS) -E pchast.h -o $@

pchast_M100.o : pchast.c
	$(CC) -include stdint.h -include pchast_M100_config.h -include pchast.h -c $(CFLAGS) pchast.c -o $@

pchast_S1000.o : pchast.c
	$(CC) -include stdint.h -include pchast_S1000_config.h -include pchast.h -c $(CFLAGS) pchast.c -o $@

pchast_P100.o : pchast.c
	$(CC) -include stdint.h -include pchast_P100_config.h -include pchast.h -c $(CFLAGS) pchast.c -o $@

main.o : main.c pchast_S1000.o pchast.h pchast_S1000_config.h
	$(CC) -include pchast_S1000_config.h $(CFLAGS) -c main.c -o $@

multithreaded_test.o : multithreaded_test.c pchast_M100.o pchast.h pchast_M100_config.h
	$(CC) -include pchast_M100_config.h $(CFLAGS) -c multithreaded_test.c -o $@

multiprocess_test.o : multiprocess_test.c pchast_P100.o pchast_M100_config.h pchast_P100.h
	$(CC) $(CFLAGS) -c multiprocess_test.c -o $@

pchast_test: main.o pchast_S1000.o pchast.h pchast_S1000_config.h
	$(CC) -include pchast_S1000_config.h -include pchast.h $(CFLAGS) main.o pchast_S1000.o -o $@

pchast_multithreaded: multithreaded_test.o pchast_M100.o pchast.h pchast_M100_config.h
	$(CC) -include pchast_M100_config.h -include pchast.h $(CFLAGS) -lpthread multithreaded_test.o pchast_M100.o -o $@

pchast_2tables: headers 2tables.c pchast_M100.o pchast_S1000.o
	$(CC) $(CFLAGS) -lpthread pchast_M100.o pchast_S1000.o 2tables.c -o $@

pchast_multiprocess: multiprocess_test.o pchast_P100.o
	$(CC) $(CFLAGS) multiprocess_test.o pchast_P100.o -o $@

# NOTE using uthash is optional. To use it clone the uthash repo in the UTHASH directory, and build the uthash-related targets.
UTHASH=uthash
pchast_M100_uthash.o : pchast_uthash.c
	$(CC) -include stdint.h -include pchast_M100_config.h -include pchast.h -I$(UTHASH)/include -c $(CFLAGS) pchast_uthash.c -o $@

pchast_multithreaded_uthash: multithreaded_test.o pchast_M100_uthash.o pchast.h pchast_M100_config.h
	$(CC) -include pchast_M100_config.h -include pchast.h $(CFLAGS) -lpthread multithreaded_test.o pchast_M100_uthash.o -o $@

tests: pchast_test pchast_multithreaded pchast_2tables pchast_multiprocess

clean:
	rm -f main.o multithreaded_test.o pchast.o libpchast.a pchest_test pchast_multithreaded pchast_M100.o pchast_M100.h pchast_S1000.o pchast_S1000.h pchast_2tables pchast_P100.o pchast_P100.h pchast_multiprocess multiprocess_test.o
