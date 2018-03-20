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

dist: singlethreaded_test.c multithreaded_test.c garnish.c garnish.h garnish_M100_config.h garnish_S1000_config.h Makefile 2tables.c garnish_debug.h garnish_P100_config.h multiprocess_test.c README.md DEVELOPER.md
	tar czvf garnish.tgz $^

libgarnish.a: garnish_M100.o garnish_S1000.o garnish_P100.o
	ar rcs $@ $^

headers: garnish_M100.h garnish_S1000.h garnish_P100.h

include Makefile.targets

tests: garnish_test garnish_multithreaded garnish_2tables garnish_multiprocess

clean:
	rm -f singlethreaded_test.o multithreaded_test.o garnish.o libgarnish.a garnish_test garnish_multithreaded garnish_M100.o garnish_M100.h garnish_S1000.o garnish_S1000.h garnish_2tables garnish_P100.o garnish_P100.h garnish_multiprocess multiprocess_test.o garnish_hash_S1000.o garnish_hash_M100.o garnish_hash_P100.o
