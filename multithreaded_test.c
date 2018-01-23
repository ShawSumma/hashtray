/*
Multithreaded test of using PCHAST
Nik Sultana, University of Pennsylvania, January 2018
*/

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pchast.h"
#include "randomlib.h"


struct server_info_t {
  uint8_t idx;
  bool shutdown;
  int seed;
  // FIXME include connection statistics
};
#define NUM_SERVERS 10
struct table * tbl = NULL;
struct server_info_t server_info[NUM_SERVERS];
pthread_t tid[NUM_SERVERS];

// Maximum amount of time before we finish serving a connection and the arrival of a new one.
#define MAX_SLEEP 10
// Maximum amount of time that an adversary can stall a connection .
#define MAX_STALL 10
// FIXME #defined GOOD_VS_BAD_LIKELIHOOD

struct sigaction sigact;

static void
sig_handler (int signal) {
  static int attempt = 0;
  if (SIGINT == signal) {
    if (0 == attempt) {
      fprintf(stderr, "Shutting down threads, this can take up to %d seconds. Please wait...\n",
          MAX_SLEEP + MAX_STALL);
      for (int i = 0; i < NUM_SERVERS; i++) {
        server_info[i].shutdown = true;
      }
      attempt += 1;
    } else {
      fprintf(stderr, "Cancelling threads...\n");
      for (int i = 0; i < NUM_SERVERS; i++) {
        pthread_cancel(tid[i]);
      }
    }
  }
}

void
init_signals(void) {
  sigact.sa_handler = &sig_handler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  sigaction(SIGINT, &sigact, (struct sigaction *) NULL);
}

void
exit_handler(void) {
  sigemptyset(&sigact.sa_mask);
}

void *
server_main(void * arg) {
  assert(MAX_SLEEP < INT_MAX);
  struct server_info_t * info = (struct server_info_t *)arg;
  printf("Server %d active\n", info->idx);

  enum outcome o;
  while (! info->shutdown) {
    sleep((uint32_t)RandomInt(0, MAX_SLEEP));

    DATA_TYPE host_id = (DATA_TYPE)RandomInt(0, INT_MAX); // FIXME weigh this to generate good and bad consistently
    VALUE_TYPE classification;
    o = lookup(tbl, host_id, &classification); // FIXME could time this.
    switch (o) {
    case OK:
      // FIXME could check for collision.
      break;
    case NOT_FOUND:
      // FIXME could check for whether this was kicked out.
      // FIXME here could take the hit, to sleep according to whether host_id
      //       relates to good or bad. This would reduce the throughput of the
      //       model according to the amount of host_id's controlled by the
      //       adversary, and our difficulty classifying them.
      // FIXME Compare this against not having the classification in place.
      o = insert(tbl, host_id, classification); // FIXME could time this.

      // FIXME could also model reclassification at some sampling rate, to
      //       make use of the "delete" feature of this data structure.
      break;
    default:
      assert(0);
    }
  }

  return NULL;
}

int
main()
{
  atexit(exit_handler);
  init_signals();
  init_prng(PRNG_SEED);
  RandomInitialise(1802,9373); // These values were suggested in randomlib.c

  tbl = create_table();

  for (int i = 0; i < NUM_SERVERS; i++) {
    server_info[i].seed = RandomInt(0, INT_MAX);
    server_info[i].shutdown = false;
    server_info[i].idx = (uint8_t)i;
    int error = pthread_create(&(tid[i]), NULL, &server_main, (void *)&(server_info[i]));
    if (error) {
      fprintf(stderr, "pthread_create: %s\n", strerror(error));
      exit(1);
    }
  }

  for (int i = 0; i < NUM_SERVERS; i++) {
    pthread_join(tid[i], NULL);
  }

  destroy_table(tbl);
  // FIXME print output stats from server_info_t

  printf("done\n");
  return 0;
}
