# Partial-key cuckoo hash.
# (aka A cuckoo filter with a value associated with each fingerprint)
# Nik Sultana, University of Pennsylvania, November 2017

ifdef DEBUGGING
DEBUGGING=-O0 -g -DREMEMBER_LOSS -DREMEMBER_COLLISIONS -DPCHAST_ASSERT \
    -DLOG_INSERTS -DDESCRIBE_COLLISIONS
else
DEBUGGING=-O3
endif

# from https://stackoverflow.com/questions/154630/recommended-gcc-warning-options-for-c
CFLAGS+=-Wall -Wextra -Wformat=2 -Wswitch-default -Wcast-align -Wpointer-arith \
    -Wbad-function-cast -Wstrict-prototypes -Winline -Wundef -Wnested-externs \
    -Wcast-qual -Wshadow -Wwrite-strings -Wconversion -Wunreachable-code \
    -Wstrict-aliasing=2 -fno-common -fstrict-aliasing \
    -std=c99 -pedantic \
    $(DEBUGGING) \
    -DMULTITHREADED

libpchast.a: pchast.o
	ar rcs $@ $^

pchast_M100.h : pchast.h pchast_M100_config.h
	$(CC) -include pchast_M100_config.h $(CFLAGS) -E pchast.h -o $@

pchast_S10000.h : pchast.h pchast_S10000_config.h
	$(CC) -include pchast_S10000_config.h $(CFLAGS) -E pchast.h -o $@

pchast_M100.o : pchast_M100.h pchast.c
	$(CC) -include pchast_M100_config.h -include pchast.h -c $(CFLAGS) pchast.c -o $@

pchast_S10000.o : pchast_S10000.h pchast.c
	$(CC) -include pchast_S10000_config.h -include pchast.h -c $(CFLAGS) pchast.c -o $@

main.o : main.c pchast_S10000.o pchast.h pchast_S10000_config.h
	$(CC) -include pchast_S10000_config.h $(CFLAGS) -c main.c -o $@

multithreaded_test.o : multithreaded_test.c pchast_M100.o pchast.h pchast_M100_config.h
	$(CC) -include pchast_M100_config.h $(CFLAGS) -c multithreaded_test.c -o $@
#	$(CC) -include pchast_M100_config.h $(CFLAGS) -c $^ -o $@

.PHONY: clean tests

#pchast_test: main.o pchast_S10000.o
#	$(CC) $(CFLAGS) $^ -o $@
pchast_test: main.o pchast_S10000.o pchast.h pchast_S10000_config.h
	$(CC) -include pchast_S10000_config.h -include pchast.h $(CFLAGS) -lpthread main.o pchast_S10000.o -o $@

#pchast_multithreaded: multithreaded_test.o pchast_M100.o pchast.h pchast_M100_config.h
pchast_multithreaded: multithreaded_test.o pchast_M100.o pchast.h pchast_M100_config.h
	$(CC) -include pchast_M100_config.h -include pchast.h $(CFLAGS) -lpthread multithreaded_test.o pchast_M100.o -o $@

tests: pchast_test pchast_multithreaded

clean:
	rm -f main.o multithreaded_test.o pchast.o libpchast.a pchest_test pchast_multithreaded pchast_M100.o pchast_M100.h pchast_S10000.o pchast_S10000.h
