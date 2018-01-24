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

// Duration of the simulation in real time (seconds).
#define SIM_DURATION 5
// The number of distinct hosts on the network.
#define NUM_HOSTS 100
// The percentage of NUM_HOSTS that are "good".
#define PERCENTAGE_GOOD_HOSTS 80
// Maximum amount of time before we finish serving a connection and the arrival of a new one.
#define MAX_SLEEP 0
// Bounds on the amount of time that an adversary can stall a connection.
#define MIN_STALL 2
#define MAX_STALL 10
// Maximum number of connections a host can have with our servers. (These connections may be distributed among different servers.)
#define MAX_CONNS 10

struct host_info_t {
  VALUE_TYPE id;
  bool is_good;
  pthread_mutex_t lock;
  uint8_t current_num_connections;
};
static struct host_info_t host_info[NUM_HOSTS];

static void generate_hosts(void);
static void shutdown_hosts(void);
static void print_host_info(bool);

static void lock_host(struct host_info_t *);
static void unlock_host(struct host_info_t *);

static void
print_host_info(bool detailed) {
  int good = 0;
  int bad = 0;
  for (int i = 0; i < NUM_HOSTS; i++) {
    if (detailed) {
      printf("%d : %d\n", host_info[i].id, host_info[i].is_good);
    }

    if (host_info[i].is_good) {
      good += 1;
    } else {
      bad += 1;
    }
  }
  printf("good hosts=%d; bad hosts=%d\n", good, bad);
}

static void
generate_hosts(void) {
  int error;
  for (int i = 0; i < NUM_HOSTS; i++) {

    // Ensure the id is unique.
    while (true) { // FIXME risk of infinite loop?
      host_info[i].id = (VALUE_TYPE)rand_range(0, RAND_MAX);
      bool duplicate = false;
      for (int j = 0; j < i; j++) {
        if (host_info[i].id == host_info[j].id) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) {
        break;
      }
    }

    host_info[i].current_num_connections = 0;

    int goodness = rand_range(0, 100);
    host_info[i].is_good = (goodness <= PERCENTAGE_GOOD_HOSTS);

    error = pthread_mutex_init(&(host_info[i].lock), NULL);
    assert(!error);
  }
}

static void
shutdown_hosts(void) {
  int error;
  for (int i = 0; i < NUM_HOSTS; i++) {
    error = pthread_mutex_destroy(&(host_info[i].lock));
    assert(!error);
  }
}

static void
lock_host(struct host_info_t * hinfo) {
  int error = pthread_mutex_lock(&(hinfo->lock));
  assert(!error);
}

static void
unlock_host(struct host_info_t * hinfo) {
  int error = pthread_mutex_unlock(&(hinfo->lock));
  assert(!error);
}

static struct host_info_t *
pick_host(void) {
  int idx;
  while (true) { // FIXME risk of infinite loop?
    idx = rand_range(0, NUM_HOSTS - 1);
    int error = pthread_mutex_trylock(&(host_info[idx].lock));
    if (! error) {
      if (host_info[idx].current_num_connections >= MAX_CONNS) {
        error = pthread_mutex_unlock(&(host_info[idx].lock));
        assert(!error);
      } else {
        break;
      }
    }
  }
  return &(host_info[idx]);
}

// The values used to classify hosts.
#define GOOD_HOST 0
#define BAD_HOST 1

struct sigaction sigact;

static void
sig_handler (int signal) {
  static int attempt = 0;
  if (SIGALRM == signal || SIGINT == signal) {
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
  if (sigaction(SIGINT, &sigact, (struct sigaction *) NULL)) {
    perror("Cannot handle SIGINT");
  }
  if (sigaction(SIGALRM, &sigact, (struct sigaction *) NULL)) {
    perror("Cannot handle SIGALRM");
  }
}

void
exit_handler(void) {
  sigemptyset(&sigact.sa_mask);
}

#define TRIES_TO_CREATE_NEW_HOST 50
void *
server_main(void * arg) {
  assert(MAX_SLEEP < INT_MAX);
  struct server_info_t * info = (struct server_info_t *)arg;
  printf("Server %d active\n", info->idx);

  enum outcome o;
  while (! info->shutdown) {
    sleep((uint32_t)rand_range(0, MAX_SLEEP));
    struct host_info_t * hinfo = pick_host();
    hinfo->current_num_connections += 1;
    unlock_host(hinfo);

    uint32_t delay = 0;

    VALUE_TYPE classification;
    o = lookup(tbl, hinfo->id, &classification); // FIXME could time this.
    switch (o) {
    case OK:
      // FIXME could check for collision.

      if (! info->shutdown) {
        switch (classification) {
          case BAD_HOST:
#ifdef SHOW_PROGRESS
            printf("!"); fflush(stdout);
#endif // SHOW_PROGRESS
            break;
          case GOOD_HOST:
#ifdef SHOW_PROGRESS
            printf("."); fflush(stdout);
#endif // SHOW_PROGRESS
            break;
          default:
            assert(0);
        }
      }

      break;
    case NOT_FOUND:
      // FIXME could check for whether this was kicked out.
      // FIXME here could take the hit, to sleep according to whether host_id
      //       relates to good or bad. This would reduce the throughput of the
      //       model according to the amount of host_id's controlled by the
      //       adversary, and our difficulty classifying them.
      // FIXME Compare this against not having the classification in place.

      if (! info->shutdown) {
        // FIXME can make the simulator "blind" to "is_good" by only observing timings,
        //       and keeping a moving average.
        if (! hinfo->is_good) {
#ifdef SHOW_PROGRESS
          printf("-"); fflush(stdout);
#endif // SHOW_PROGRESS
          classification = BAD_HOST;
          delay = (uint32_t)rand_range(MIN_STALL, MAX_STALL);
        } else {
#ifdef SHOW_PROGRESS
          printf("+"); fflush(stdout);
#endif // SHOW_PROGRESS
          classification = GOOD_HOST;
        }
      }

      o = insert(tbl, hinfo->id, classification); // FIXME could time this.
      // FIXME could also model reclassification at some sampling rate, to
      //       make use of the "delete" feature of this data structure.
      break;
    default:
      assert(0);
    }

#ifdef REALLY_SLEEP
    sleep(delay);
#endif // REALLY_SLEEP

    lock_host(hinfo);
    hinfo->current_num_connections -= 1;
    assert(hinfo->current_num_connections >= 0);
    unlock_host(hinfo);
  }

  return NULL;
}

int
main()
{
  assert(GOOD_HOST != BAD_HOST);

  atexit(exit_handler);
  init_signals();
  srand(1802 * 9373);

  alarm(SIM_DURATION);

  generate_hosts();
  print_host_info(false);

  tbl = create_table();

  for (int i = 0; i < NUM_SERVERS; i++) {
    server_info[i].seed = rand_range(0, INT_MAX);
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
  shutdown_hosts();

  printf("done\n");
  return 0;
}
