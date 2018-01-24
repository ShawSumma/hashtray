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

static void print_server_info(void);

struct server_info_t {
  uint8_t idx;
  bool shutdown;
  int seed;

  uint32_t num_connections;
  uint32_t num_connections_good;
  uint32_t num_connections_bad;
  uint32_t tot_duration;
  uint32_t avg_duration;

  uint32_t host_unknown;
  uint32_t host_known;
  uint32_t host_classified_correct;
  uint32_t host_classified_incorrect;
};
#define NUM_SERVERS 10
struct table * tbl = NULL;
struct server_info_t server_info[NUM_SERVERS];
pthread_t tid[NUM_SERVERS];

static void
print_server_info(void) {
  printf("I\tSh\tSd\t\tNc\tNg\tNb\tTd\tAd\tHu\tHk\tCc\tCi\n");
  for (int i = 0; i < NUM_SERVERS; i++) {
  printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
      server_info[i].idx, server_info[i].shutdown, server_info[i].seed,
      server_info[i].num_connections, server_info[i].num_connections_good,
      server_info[i].num_connections_bad, server_info[i].tot_duration,
      server_info[i].avg_duration, server_info[i].host_unknown,
      server_info[i].host_known, server_info[i].host_classified_correct,
      server_info[i].host_classified_incorrect);
  }
}

// Duration of the simulation in real time (seconds).
#define SIM_DURATION 5
// The number of distinct hosts on the network.
#define NUM_HOSTS 1000
// The percentage of NUM_HOSTS that are "good".
#define PERCENTAGE_GOOD_HOSTS 80
// The likelihood that a connection comes from a good host -- i.e., the lower
// this value then the more determined is the adversary.
#define PERCENTAGE_GOOD_CONNECTION 0
// Maximum amount of time before we finish serving a connection and the arrival of a new one.
#define MAX_SLEEP 0
// Bounds on the amount of time that an adversary can stall a connection.
#define MIN_STALL 2
#define MAX_STALL 100
// Maximum number of connections a host can have with our servers. (These connections may be distributed among different servers.)
#define MAX_CONNS 100
// The quantity of units to be added to the average delay, to serve as a tolerance.
// i.e, anything above avg_duration + DELAY_TOLERANCE is classified as bad.
#define DELAY_TOLERANCE 1
// FIXME could periodically change network conditions, so even good hosts appear bad,
//       and require heeding a changing average.

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

#define MAX_ITERATIONS 100

static void
generate_hosts(void) {
  int error;
  for (int i = 0; i < NUM_HOSTS; i++) {

    // Ensure the id is unique.
    int iterations = MAX_ITERATIONS;
    for (; iterations > 0; iterations--) {
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
    assert(iterations > 0);

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
  int iterations = MAX_ITERATIONS;
  for (; iterations > 0; iterations--) {
    idx = rand_range(0, NUM_HOSTS - 1);
    int error = pthread_mutex_trylock(&(host_info[idx].lock));
    if (! error) {
      if (host_info[idx].current_num_connections < MAX_CONNS) {
        bool conn_should_be_good = (rand_range(0, 100) < PERCENTAGE_GOOD_CONNECTION);
        if ((conn_should_be_good && host_info[idx].is_good) ||
            (!conn_should_be_good && !host_info[idx].is_good)) {
          break;
        }
      }

      error = pthread_mutex_unlock(&(host_info[idx].lock));
      assert(!error);
    }
  }
  assert(iterations > 0);
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
  struct server_info_t * sinfo = (struct server_info_t *)arg;
  printf("Server %d active\n", sinfo->idx);

  enum outcome o;
  while (! sinfo->shutdown) {
    sleep((uint32_t)rand_range(0, MAX_SLEEP));
    struct host_info_t * hinfo = pick_host();
    hinfo->current_num_connections += 1;
    unlock_host(hinfo);

    sinfo->num_connections += 1;

    if (hinfo->is_good) {
      sinfo->num_connections_good += 1;
    } else {
      sinfo->num_connections_bad += 1;
    }
    assert(sinfo->num_connections = sinfo->num_connections_good + sinfo->num_connections_bad);

    uint32_t delay = 0;

    VALUE_TYPE classification = (VALUE_TYPE)(-1);
#ifdef USE_PCHAST
    o = lookup(tbl, hinfo->id, &classification); // FIXME could time this.
#else
    o = NOT_FOUND;
#endif // USE_PCHAST
    switch (o) {
    case OK:
      // FIXME could check for collision.

      sinfo->host_known += 1;

      if (! sinfo->shutdown) {
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
      sinfo->host_unknown += 1;
      // FIXME could check for whether this was kicked out.
      // FIXME here could take the hit, to sleep according to whether host_id
      //       relates to good or bad. This would reduce the throughput of the
      //       model according to the amount of host_id's controlled by the
      //       adversary, and our difficulty classifying them.
      // FIXME Compare this against not having the classification in place.

      if (! sinfo->shutdown) {
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
#ifndef PERFECT_GOOD
          delay = (uint32_t)rand_range(0, MIN_STALL); // We add some noise, since even "good" hosts might appear imperfect.
#endif // PERFECT_GOOD
          classification = GOOD_HOST;
        }
      }

#ifdef USE_PCHAST
#ifndef USE_PERFECT_CLASSIFIER
      // We redefine "classification" based on observed time, rather than based on ground truth (hinfo->is_good).
      if (delay > sinfo->avg_duration + DELAY_TOLERANCE) {
        classification = BAD_HOST;
        if (hinfo->is_good) {
          sinfo->host_classified_incorrect += 1;
        }
      } else {
        classification = GOOD_HOST;
        if (hinfo->is_good) {
          sinfo->host_classified_correct += 1;
        }
      }
#endif // USE_PERFECT_CLASSIFIER

      o = insert(tbl, hinfo->id, classification); // FIXME could time this.
#endif // USE_PCHAST
      // FIXME could also model reclassification at some sampling rate, to
      //       make use of the "delete" feature of this data structure.
      break;
    default:
      assert(0);
    }

    sinfo->tot_duration += delay;
    sinfo->avg_duration = sinfo->tot_duration / sinfo->num_connections;
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
  shutdown_hosts();

  print_server_info();

  printf("done\n");
  return 0;
}
