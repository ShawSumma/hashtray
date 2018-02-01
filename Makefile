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
    -DMULTITHREADED -lpthread

libpchast.a: pchast.o
	ar rcs $@ $^

%.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $^

.PHONY: clean tests

pchast_test: main.o pchast.o
	$(CC) $(CFLAGS) -o $@ $^

pchast_multithread: multithreaded_test.o pchast.o
	$(CC) $(CFLAGS) -o $@ $^

tests: pchast_test pchast_multithread

clean:
	rm main.o multithreaded_test.o pchast.o libpchast.a pchest_test pchast_multithread
