
CC ?= cc
AR ?= ar
ARFLAGS := rcs

UTHASH ?= uthash
SIPHASH ?= SipHash

WARN_FLAGS := -Wall -Wextra -Wformat=2 -Wswitch-default -Wcast-align \
  -Wpointer-arith -Wbad-function-cast -Wstrict-prototypes -Winline \
  -Wundef -Wnested-externs -Wcast-qual -Wshadow -Wwrite-strings \
  -Wconversion -Wunreachable-code -Wstrict-aliasing=2

DEBUG_FLAGS := -O0 -g \
  -DREMEMBER_LOSS -DREMEMBER_COLLISIONS -DHASHTRAY_ASSERT \
  -DHASHTRAY_LOG_INSERTS -DHASHTRAY_DESCRIBE_COLLISIONS

CFLAGS += -D_POSIX_C_SOURCE=200112L \
  ${WARN_FLAGS} \
  -fno-common -fstrict-aliasing \
  -std=c99 -pedantic

DEPFLAGS = -MMD -MP

COMPILE = ${CC} -Isrc -c ${CFLAGS} ${DEPFLAGS}
LINK = ${CC} ${CFLAGS}
GENH = ${CC} ${CFLAGS} -E -P -D_STDINT_H -D_GCC_WRAP_STDINT_H

M100_CONFIG := config/m100.h
S1000_CONFIG := config/s1000.h
P100_CONFIG := config/p100.h

M100_OBJS := build/hashtray_M100.o build/hashtray_hash_M100.o build/lock_pthread_M100.o
S1000_OBJS := build/hashtray_S1000.o build/hashtray_hash_S1000.o build/lock_none_S1000.o
P100_OBJS := build/hashtray_P100.o build/hashtray_hash_P100.o build/lock_semaphore_P100.o

# Library objects

build/hashtray_%.o: src/hashtray.c | build
	${COMPILE} -include ${${*}_CONFIG} ${<} -o ${@}

build/hashtray_hash_%.o: src/hashtray_hash.c | build
	${COMPILE} -include ${${*}_CONFIG} ${<} -o ${@}

# Lock backends

build/lock_none_S1000.o: src/lock_none.c | build
	${COMPILE} -include ${S1000_CONFIG} ${<} -o ${@}

build/lock_pthread_M100.o: src/lock_pthread.c | build
	${COMPILE} -include ${M100_CONFIG} ${<} -o ${@}

build/lock_semaphore_P100.o: src/lock_semaphore.c | build
	${COMPILE} -include ${P100_CONFIG} ${<} -o ${@}

# Generated headers

build/hashtray_M100.h: src/hashtray.h ${M100_CONFIG} | build
	${GENH} -include ${M100_CONFIG} src/hashtray.h -o ${@}

build/hashtray_S1000.h: src/hashtray.h ${S1000_CONFIG} | build
	${GENH} -include ${S1000_CONFIG} src/hashtray.h -o ${@}

build/hashtray_P100.h: src/hashtray.h ${P100_CONFIG} | build
	${GENH} -include ${P100_CONFIG} src/hashtray.h -o ${@}

# Static library

build/libhashtray.a: ${M100_OBJS} ${S1000_OBJS} ${P100_OBJS}
	${AR} ${ARFLAGS} ${@} $^

# Test objects

build/singlethreaded_test.o: test/singlethreaded_test.c | build
	${COMPILE} -include ${S1000_CONFIG} ${<} -o ${@}

build/multithreaded_test.o: test/multithreaded_test.c | build
	${COMPILE} -include ${M100_CONFIG} ${<} -o ${@}

build/multiprocess_test.o: test/multiprocess_test.c build/hashtray_P100.h | build
	${CC} -Ibuild -c ${CFLAGS} ${DEPFLAGS} ${<} -o ${@}

build/2tables.o: test/2tables.c build/hashtray_M100.h build/hashtray_S1000.h | build
	${CC} -Ibuild -c ${CFLAGS} ${DEPFLAGS} ${<} -o ${@}

# Test binaries

build/hashtray_test: build/singlethreaded_test.o ${S1000_OBJS}
	${LINK} $^ -o ${@}

build/hashtray_multithreaded: build/multithreaded_test.o ${M100_OBJS}
	${LINK} -lpthread $^ -o ${@}

build/hashtray_2tables: build/2tables.o ${M100_OBJS} ${S1000_OBJS}
	${LINK} -lpthread $^ -o ${@}

build/hashtray_multiprocess: build/multiprocess_test.o ${P100_OBJS}
	${LINK} $^ -o ${@}

# Optional: uthash wrapper

build/hashtray_M100_uthash.o: src/hashtray_uthash.c | build
	${COMPILE} -include ${M100_CONFIG} -I${UTHASH}/include ${<} -o ${@}

build/hashtray_multithreaded_uthash: build/multithreaded_test.o build/hashtray_M100_uthash.o \
  build/hashtray_hash_M100.o
	${LINK} -lpthread $^ -o ${@}

# Optional: SipHash

build/hashtray_siphash_M100.o: src/sip_hash.c | build
	${COMPILE} -include ${M100_CONFIG} ${<} -o ${@}

build/hashtray_multithreaded_siphash: build/multithreaded_test.o build/hashtray_M100.o \
  build/hashtray_siphash_M100.o ${SIPHASH}/halfsiphash.o build/lock_pthread_M100.o
	${LINK} -lpthread $^ -o ${@}

# Aggregate targets

.PHONY: all tests test headers clean dist debug

all: build/libhashtray.a

tests: build/hashtray_test build/hashtray_multithreaded build/hashtray_2tables build/hashtray_multiprocess

headers: build/hashtray_M100.h build/hashtray_S1000.h build/hashtray_P100.h

test: tests
	@echo "--- singlethreaded ---"
	@build/hashtray_test
	@echo "--- multithreaded ---"
	@build/hashtray_multithreaded
	@echo "--- 2tables ---"
	@build/hashtray_2tables
	@echo "--- multiprocess ---"
	@build/hashtray_multiprocess
	@echo "--- all tests passed ---"

debug:
	${MAKE} CFLAGS+="${DEBUG_FLAGS}"

# Distribution

DIST_FILES := \
  src/hashtray.c src/hashtray.h src/hashtray_hash.c src/hashtray_hash.h \
  src/hashtray_debug.h src/hashtray_assert.h src/common.h \
  src/lock.h src/lock_none.c src/lock_pthread.c src/lock_semaphore.c \
  config/m100.h config/s1000.h config/p100.h \
  src/hashtray_uthash.c src/sip_hash.c \
  test/singlethreaded_test.c test/multithreaded_test.c \
  test/multiprocess_test.c test/2tables.c \
  Makefile README.md LICENSE

dist: hashtray.tgz

hashtray.tgz: ${DIST_FILES}
	tar czvf $^ ${@}

clean:
	rm -rf build hashtray.tgz

build:
	mkdir -p ${@}

-include build/*.d
