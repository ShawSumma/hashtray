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

// Number of servers in the simulation. These will accept connections concurrently.
#define NUM_SERVERS 10
// I define this to help us see the "hit" in the number of connections that can
// be served when the system is under attack vs when it isn't (and when the
// mitigation is in place vs when it isn't). I don't normally use it since it
// slows down the simulation.
//#define REALLY_SLEEP
// I defined PERFECT_GOOD otherwise the results are confusing when PERCENTAGE_GOOD_CONNECTION=100
#define PERFECT_GOOD
// Duration of the simulation in real time (seconds).
#define SIM_DURATION_SECS 5
// The number of distinct hosts on the network.
#undef SIM_DURATION_IN_SECONDS
// Duration of the simulation in number of connections.
#define SIM_DURATION_CONNS 1000000
#define SIM_DURATION_IN_CONNECTIONS
#define NUM_HOSTS 50000
// The percentage of NUM_HOSTS that are "good".
#define PERCENTAGE_GOOD_HOSTS 80
// The likelihood that a connection comes from a good host -- i.e., the lower
// this value then the more determined is the adversary.
//#define PERCENTAGE_GOOD_CONNECTION 5
int PERCENTAGE_GOOD_CONNECTION = -1;
// Maximum amount of time before we finish serving a connection and the arrival of a new one.
// So MAX_SLEEP==0 means we're modelling a very demanding environment where connections
// are coming in all the time.
#define MAX_SLEEP 0
// Bounds on the amount of time that an adversary can stall a connection.
#define MIN_STALL 2
#define MAX_STALL 5
// Maximum number of connections a host can have with our servers. (These connections may be distributed among different servers.)
#define MAX_CONNS 1
// The quantity of units to be added to the average delay, to serve as a tolerance.
// i.e, anything above avg_duration + DELAY_TOLERANCE is classified as bad.
#define DELAY_TOLERANCE 1
// FIXME could periodically change network conditions, so even good hosts appear bad,
//       and require heeding a changing average.

// Bound on loops.
#define MAX_ITERATIONS 500

// The values used to classify hosts.
#define GOOD_HOST 0
#define BAD_HOST 1

#if !defined(SIM_DURATION_IN_SECONDS) && !defined(SIM_DURATION_IN_CONNECTIONS)
#error "Must define SIM_DURATION_IN_SECONDS or SIM_DURATION_IN_CONNECTIONS"
#endif


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
struct table * tbl = NULL;
struct server_info_t server_info[NUM_SERVERS];
pthread_t tid[NUM_SERVERS];

#ifdef SIM_DURATION_IN_CONNECTIONS
uint32_t connections_countdown = SIM_DURATION_CONNS;
pthread_mutex_t connections_countdown_lock;
#endif // SIM_DURATION_IN_CONNECTIONS

static void
print_server_info(void) {
  uint32_t total_num_connections = 0;
  uint32_t total_num_connections_good = 0;
  uint32_t total_num_connections_bad = 0;
  uint32_t total_tot_duration = 0;
  double average_duration = 0;

#ifdef VERBOSE
  printf("I\tSh\tSd\t\tNc\tNg\tNb\tTd\tAd\tHu\tHk\tCc\tCi\n");
#endif // VERBOSE
  for (int i = 0; i < NUM_SERVERS; i++) {
#ifdef VERBOSE
    printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
        server_info[i].idx, server_info[i].shutdown, server_info[i].seed,
        server_info[i].num_connections, server_info[i].num_connections_good,
        server_info[i].num_connections_bad, server_info[i].tot_duration,
        server_info[i].avg_duration, server_info[i].host_unknown,
        server_info[i].host_known, server_info[i].host_classified_correct,
        server_info[i].host_classified_incorrect);
#endif // VERBOSE

    total_num_connections += server_info[i].num_connections;
    total_num_connections_good += server_info[i].num_connections_good;
    total_num_connections_bad += server_info[i].num_connections_bad;
    total_tot_duration += server_info[i].tot_duration;
    average_duration = (double)total_tot_duration / (double)total_num_connections;
  }

  // Worst duration possible: if every connection to every server produced the maximum amount of stall.
  double max_stall = (double)total_num_connections * (double)MAX_STALL;
  double relative_stall = (double)total_tot_duration / max_stall;

#ifdef VERBOSE
  printf("total_num_connections=%u, total_num_connections_good=%u, total_num_connections_bad=%u, total_tot_duration=%u, average_duration=%f, max_stall=%f, relative_stall=%f\n",
    total_num_connections,
    total_num_connections_good,
    total_num_connections_bad,
    total_tot_duration,
    average_duration,
    max_stall,
    relative_stall);
#else
  printf("%d %f\n", PERCENTAGE_GOOD_CONNECTION, relative_stall);
#endif // VERBOSE
}

struct host_info_t {
  VALUE_TYPE id;
  bool is_good;
  pthread_mutex_t lock;
  uint8_t current_num_connections;
};
static struct host_info_t host_info[NUM_HOSTS];

static void generate_hosts(void);
static void shutdown_hosts(void);
#ifdef VERBOSE
static void print_host_info(bool);
#endif // VERBOSE

static void lock_host(struct host_info_t *);
static void unlock_host(struct host_info_t *);

#ifdef VERBOSE
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
#endif // VERBOSE

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

    int goodness = rand_range(1, 100);
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
        bool conn_should_be_good = (rand_range(1, 100) <= PERCENTAGE_GOOD_CONNECTION);
        if (conn_should_be_good == host_info[idx].is_good) {
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
      alarm(SIM_DURATION_SECS); // Wait further before cancelling.
    } else {
      fprintf(stderr, "Cancelling threads...\n");
      for (int i = 0; i < NUM_SERVERS; i++) {
        pthread_cancel(tid[i]);
      }
      alarm(SIM_DURATION_SECS); // Keep trying to cancel.
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

void *
server_main(void * arg) {
  assert(MAX_SLEEP < INT_MAX);
  struct server_info_t * sinfo = (struct server_info_t *)arg;
#ifdef VERBOSE
  printf("Server %d active\n", sinfo->idx);
#endif // VERBOSE

  int ignored;
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &ignored);
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &ignored);

  int error;

  enum outcome o;
  while (! sinfo->shutdown) {
    sleep((uint32_t)rand_range(0, MAX_SLEEP));

#ifdef SIM_DURATION_IN_CONNECTIONS
    error = pthread_mutex_lock(&connections_countdown_lock);
    assert(!error);
    if (0 == connections_countdown) {
      error = pthread_mutex_unlock(&connections_countdown_lock);
      assert(!error);
      break;
    }
    connections_countdown -= 1;
    error = pthread_mutex_unlock(&connections_countdown_lock);
    assert(!error);
#endif // SIM_DURATION_IN_CONNECTIONS

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
main(int argc, char * argv[])
{
  assert(GOOD_HOST != BAD_HOST);

  assert(2 == argc);
  PERCENTAGE_GOOD_CONNECTION = (int)strtol(argv[1], (char **)NULL, 10);

  atexit(exit_handler);
  init_signals();
  srand(1802 * 9373);

#ifdef SIM_DURATION_IN_SECONDS
  alarm(SIM_DURATION_SECS);
#endif // SIM_DURATION_IN_SECONDS

  int error;
#ifdef SIM_DURATION_IN_CONNECTIONS
  error = pthread_mutex_init(&connections_countdown_lock, NULL);
  assert(!error);
#endif // SIM_DURATION_IN_CONNECTIONS

  generate_hosts();
#ifdef VERBOSE
  print_host_info(false);
#endif // VERBOSE

  tbl = create_table();

  for (int i = 0; i < NUM_SERVERS; i++) {
    server_info[i].seed = rand_range(0, INT_MAX);
    server_info[i].shutdown = false;
    server_info[i].idx = (uint8_t)i;
    error = pthread_create(&(tid[i]), NULL, &server_main, (void *)&(server_info[i]));
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

#ifdef SIM_DURATION_IN_CONNECTIONS
  error = pthread_mutex_destroy(&connections_countdown_lock);
  assert(!error);
#endif // SIM_DURATION_IN_CONNECTIONS

  print_server_info();

#ifdef VERBOSE
  printf("done\n");
#endif // VERBOSE
  return 0;
}
