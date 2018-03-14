# libgarnish
# Nik Sultana, University of Pennsylvania, November 2017

ifdef DEBUGGING
DEBUGGING=-O0 -g -DREMEMBER_LOSS -DREMEMBER_COLLISIONS -DGARNISH_ASSERT \
    -DGARNISH_LOG_INSERTS -DGARNISH_DESCRIBE_COLLISIONS -include stdbool.h
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

dist: singlethreaded_test.c multithreaded_test.c garnish.c garnish.h garnish_M100_config.h garnish_S1000_config.h Makefile 2tables.c garnish_debug.h garnish_P100_config.h multiprocess_test.c README.md
	tar czvf garnish.tgz $^

libgarnish.a: garnish_M100.o garnish_S1000.o garnish_P100.o
	ar rcs $@ $^

headers: garnish_M100.h garnish_S1000.h garnish_P100.h

garnish_M100.h : garnish.h garnish_M100_config.h
	$(CC) -include garnish_M100_config.h $(CFLAGS) -E garnish.h -o $@

garnish_S1000.h : garnish.h garnish_S1000_config.h
	$(CC) -include garnish_S1000_config.h $(CFLAGS) -E garnish.h -o $@

garnish_P100.h : garnish.h garnish_P100_config.h
	$(CC) -include garnish_P100_config.h $(CFLAGS) -E garnish.h -o $@

garnish_M100.o : garnish.c
	$(CC) -include stdint.h -include garnish_M100_config.h -include garnish.h -c $(CFLAGS) garnish.c -o $@

garnish_S1000.o : garnish.c
	$(CC) -include stdint.h -include garnish_S1000_config.h -include garnish.h -c $(CFLAGS) garnish.c -o $@

garnish_P100.o : garnish.c
	$(CC) -include stdint.h -include garnish_P100_config.h -include garnish.h -c $(CFLAGS) garnish.c -o $@

singlethreaded_test.o : singlethreaded_test.c garnish_S1000.o garnish.h garnish_S1000_config.h
	$(CC) -include garnish_S1000_config.h $(CFLAGS) -c singlethreaded_test.c -o $@

multithreaded_test.o : multithreaded_test.c garnish_M100.o garnish.h garnish_M100_config.h
	$(CC) -include garnish_M100_config.h $(CFLAGS) -c multithreaded_test.c -o $@

multiprocess_test.o : multiprocess_test.c garnish_P100.o garnish_M100_config.h garnish_P100.h
	$(CC) $(CFLAGS) -c multiprocess_test.c -o $@

garnish_test: singlethreaded_test.o garnish_S1000.o garnish.h garnish_S1000_config.h
	$(CC) -include garnish_S1000_config.h -include garnish.h $(CFLAGS) singlethreaded_test.o garnish_S1000.o -o $@

garnish_multithreaded: multithreaded_test.o garnish_M100.o garnish.h garnish_M100_config.h
	$(CC) -include garnish_M100_config.h -include garnish.h $(CFLAGS) -lpthread multithreaded_test.o garnish_M100.o -o $@

garnish_2tables: headers 2tables.c garnish_M100.o garnish_S1000.o
	$(CC) $(CFLAGS) -lpthread garnish_M100.o garnish_S1000.o 2tables.c -o $@

garnish_multiprocess: multiprocess_test.o garnish_P100.o
	$(CC) $(CFLAGS) multiprocess_test.o garnish_P100.o -o $@

# NOTE using uthash is optional. To use it clone the uthash repo in the UTHASH directory, and build the uthash-related targets. See DEVELOPER.md for more info.
UTHASH=uthash
garnish_M100_uthash.o : garnish_uthash.c
	$(CC) -include stdint.h -include garnish_M100_config.h -include garnish.h -I$(UTHASH)/include -c $(CFLAGS) garnish_uthash.c -o $@

garnish_multithreaded_uthash: multithreaded_test.o garnish_M100_uthash.o garnish.h garnish_M100_config.h
	$(CC) -include garnish_M100_config.h -include garnish.h $(CFLAGS) -lpthread multithreaded_test.o garnish_M100_uthash.o -o $@

tests: garnish_test garnish_multithreaded garnish_2tables garnish_multiprocess

clean:
	rm -f singlethreaded_test.o multithreaded_test.o garnish.o libgarnish.a garnish_test garnish_multithreaded garnish_M100.o garnish_M100.h garnish_S1000.o garnish_S1000.h garnish_2tables garnish_P100.o garnish_P100.h garnish_multiprocess multiprocess_test.o
