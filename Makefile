# libhashtray
# Nik Sultana, University of Pennsylvania, November 2017

ifdef DEBUGGING
DEBUGGING=-O0 -g -DREMEMBER_LOSS -DREMEMBER_COLLISIONS -DHASHTRAY_ASSERT \
    -DHASHTRAY_LOG_INSERTS -DHASHTRAY_DESCRIBE_COLLISIONS -include stdbool.h
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

dist: singlethreaded_test.c multithreaded_test.c hashtray.c hashtray.h hashtray_M100_config.h hashtray_S1000_config.h Makefile 2tables.c hashtray_debug.h hashtray_P100_config.h multiprocess_test.c README.md DEVELOPER.md
	tar czvf hashtray.tgz $^

libhashtray.a: hashtray_M100.o hashtray_S1000.o hashtray_P100.o
	ar rcs $@ $^

headers: hashtray_M100.h hashtray_S1000.h hashtray_P100.h

include Makefile.targets

tests: hashtray_test hashtray_multithreaded hashtray_2tables hashtray_multiprocess

clean:
	rm -f singlethreaded_test.o multithreaded_test.o hashtray.o libhashtray.a hashtray_test hashtray_multithreaded hashtray_M100.o hashtray_M100.h hashtray_S1000.o hashtray_S1000.h hashtray_2tables hashtray_P100.o hashtray_P100.h hashtray_multiprocess multiprocess_test.o hashtray_hash_S1000.o hashtray_hash_M100.o hashtray_hash_P100.o \
		hashtray_M100_uthash.o hashtray_multithreaded_siphash hashtray_siphash_M100.o
