// Multithreaded test of libhashtray.
// Simulates requests being serviced in a network of hosts, using the table to
// remember hostile hosts.

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hashtray.h"

static bool USE_HASHTRAY = false;
#define NUM_SERVERS 10
#define PERFECT_GOOD
#define SIM_DURATION_SECS 5
#undef SIM_DURATION_IN_SECONDS
#define SIM_DURATION_CONNS 1000000
#define SIM_DURATION_IN_CONNECTIONS
#define MAX_NUM_HOSTS 10000
static int32_t NUM_HOSTS = 10000;
static int32_t PERCENTAGE_GOOD_HOSTS = 80;
static int32_t PERCENTAGE_GOOD_CONNECTION = 5;
#define MAX_SLEEP 0
static int32_t MIN_STALL = 5;
static int32_t MAX_STALL = 5;
#define MAX_CONNS 1
#define DELAY_TOLERANCE 1
#define MAX_ITERATIONS 500
#define RAND_SEED 1802 * 9373
#define GOOD_HOST 0
#define BAD_HOST 1

#if (GOOD_HOST == BAD_HOST)
#error "GOOD_HOST and BAD_HOST must have different values"
#endif

#if !defined(SIM_DURATION_IN_SECONDS) && !defined(SIM_DURATION_IN_CONNECTIONS)
#error "Must define SIM_DURATION_IN_SECONDS or SIM_DURATION_IN_CONNECTIONS"
#endif

typedef struct server_info_t server_info_t;

struct server_info_t {
  uint8_t idx;
  bool shutdown;
  int32_t seed;
  bool fault;
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

static HASHTRAY(table_t) *tbl = NULL;
static server_info_t server_info[NUM_SERVERS];
static pthread_t tid[NUM_SERVERS];

#if defined(SIM_DURATION_IN_CONNECTIONS)
static uint32_t connections_countdown = SIM_DURATION_CONNS;
static pthread_mutex_t connections_countdown_lock;
#endif

typedef struct host_info_t host_info_t;

struct host_info_t {
  HASHTRAY(value_t) id;
  bool is_good;
  pthread_mutex_t lock;
  uint8_t current_num_connections;
};

static host_info_t host_info[MAX_NUM_HOSTS];

static void print_server_info(void) {
  uint32_t total_num_connections = 0;
  uint32_t total_num_connections_good = 0;
  uint32_t total_num_connections_bad = 0;
  uint32_t total_tot_duration = 0;
  double average_duration = 0;
  bool fault_occurred = false;

  for (int32_t i = 0; i < NUM_SERVERS; i++) {
    fault_occurred |= server_info[i].fault;
    total_num_connections += server_info[i].num_connections;
    total_num_connections_good += server_info[i].num_connections_good;
    total_num_connections_bad += server_info[i].num_connections_bad;
    total_tot_duration += server_info[i].tot_duration;
    average_duration = (double) total_tot_duration / (double) total_num_connections;
  }

  double max_stall = (double) total_num_connections * (double) MAX_STALL;
  double relative_stall = (double) total_tot_duration / max_stall;

  #if defined(VERBOSE)
  printf("I Sh Sd Nc Ng Nb Td Ad Hu Hk Cc Ci F\n");
  for (int32_t i = 0; i < NUM_SERVERS; i++) {
    printf("I=%d Sh=%d Sd=%d Nc=%d Ng=%d Nb=%d Td=%d Ad=%d Hu=%d Hk=%d Cc=%d Ci=%d F=%d\n",
      server_info[i].idx,
      server_info[i].shutdown,
      server_info[i].seed,
      server_info[i].num_connections,
      server_info[i].num_connections_good,
      server_info[i].num_connections_bad,
      server_info[i].tot_duration,
      server_info[i].avg_duration,
      server_info[i].host_unknown,
      server_info[i].host_known,
      server_info[i].host_classified_correct,
      server_info[i].host_classified_incorrect,
      server_info[i].fault);
  }
  printf("total_num_connections=%u ", total_num_connections);
  printf("total_num_connections_good=%u ", total_num_connections_good);
  printf("total_num_connections_bad=%u ", total_num_connections_bad);
  printf("total_tot_duration=%u ", total_tot_duration);
  printf("average_duration=%f ", average_duration);
  printf("max_stall=%f ", max_stall);
  printf("relative_stall=%f ", relative_stall);
  printf("fault_occurred=%d\n", fault_occurred);
  #else
  (void) average_duration;
  if (!fault_occurred) {
    printf("%d %f\n", PERCENTAGE_GOOD_CONNECTION, relative_stall);
  } else {
    printf("%d nan\n", PERCENTAGE_GOOD_CONNECTION);
  }
  #endif
}

#if defined(VERBOSE)
static void print_host_info(bool detailed) {
  int32_t good = 0;
  int32_t bad = 0;
  for (int32_t i = 0; i < NUM_HOSTS; i++) {
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
#endif

static void generate_hosts(void) {
  for (int32_t i = 0; i < NUM_HOSTS; i++) {
    host_info[i].id = (HASHTRAY(value_t)) i;
    host_info[i].current_num_connections = 0;
    int32_t goodness = HASHTRAY(rand_range)(1, 100);
    host_info[i].is_good = (goodness <= PERCENTAGE_GOOD_HOSTS);
    bool error = pthread_mutex_init(&(host_info[i].lock), NULL);
    assert(!error);
  }
}

static void shutdown_hosts(void) {
  for (int32_t i = 0; i < NUM_HOSTS; i++) {
    bool error = pthread_mutex_destroy(&(host_info[i].lock));
    assert(!error);
  }
}

static void lock_host(host_info_t *hinfo) {
  bool error = pthread_mutex_lock(&(hinfo->lock));
  assert(!error);
}

static void unlock_host(host_info_t *hinfo) {
  bool error = pthread_mutex_unlock(&(hinfo->lock));
  assert(!error);
}

static host_info_t *pick_host(void) {
  int32_t idx = 0;
  int32_t iterations = MAX_ITERATIONS;
  for (; iterations > 0; iterations--) {
    idx = HASHTRAY(rand_range)(0, NUM_HOSTS - 1);
    bool error = pthread_mutex_trylock(&(host_info[idx].lock));
    if (!error) {
      if (host_info[idx].current_num_connections < MAX_CONNS) {
        bool conn_should_be_good = (HASHTRAY(rand_range)(1, 100) <= PERCENTAGE_GOOD_CONNECTION);
        if (conn_should_be_good == host_info[idx].is_good) {
          break;
        }
      }
      error = pthread_mutex_unlock(&(host_info[idx].lock));
      assert(!error);
    }
  }

  if (iterations > 0) {
    return &(host_info[idx]);
  } else {
    return NULL;
  }
}

static struct sigaction sigact;

static void sig_handler(int32_t signal) {
  static int32_t attempt = 0;
  if (signal == SIGALRM || signal == SIGINT) {
    if (attempt == 0) {
      fprintf(stderr, "Shutting down threads, this can take up to %d seconds. Please wait...\n", MAX_SLEEP + MAX_STALL);
      for (int32_t i = 0; i < NUM_SERVERS; i++) {
        server_info[i].shutdown = true;
      }
      attempt += 1;
      alarm(SIM_DURATION_SECS);
    } else {
      fprintf(stderr, "Cancelling threads...\n");
      for (int32_t i = 0; i < NUM_SERVERS; i++) {
        pthread_cancel(tid[i]);
      }
      alarm(SIM_DURATION_SECS);
    }
  }
}

static void init_signals(void) {
  sigact.sa_handler = &sig_handler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  if (sigaction(SIGINT, &sigact, NULL)) {
    perror("Cannot handle SIGINT");
  }
  if (sigaction(SIGALRM, &sigact, NULL)) {
    perror("Cannot handle SIGALRM");
  }
}

static void exit_handler(void) {
  sigemptyset(&sigact.sa_mask);
}

static void *server_main(void *arg) {
  assert(MAX_SLEEP < INT_MAX);
  server_info_t *sinfo = (server_info_t *) arg;
  #if defined(VERBOSE)
  printf("Server %d active\n", sinfo->idx);
  #endif

  int32_t ignored = 0;
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &ignored);
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &ignored);

  bool error = false;

  while (!sinfo->shutdown) {
    sleep((uint32_t) HASHTRAY(rand_range)(0, MAX_SLEEP));

    #if defined(SIM_DURATION_IN_CONNECTIONS)
    error = pthread_mutex_lock(&connections_countdown_lock);
    assert(!error);
    if (connections_countdown == 0) {
      error = pthread_mutex_unlock(&connections_countdown_lock);
      assert(!error);
      break;
    }
    connections_countdown -= 1;
    error = pthread_mutex_unlock(&connections_countdown_lock);
    assert(!error);
    #endif

    host_info_t *hinfo = pick_host();
    if (hinfo == NULL) {
      sinfo->fault = true;
      break;
    }
    hinfo->current_num_connections += 1;
    unlock_host(hinfo);

    sinfo->num_connections += 1;

    if (hinfo->is_good) {
      sinfo->num_connections_good += 1;
    } else {
      sinfo->num_connections_bad += 1;
    }
    assert(sinfo->num_connections == sinfo->num_connections_good + sinfo->num_connections_bad);

    uint32_t delay = 0;

    HASHTRAY(value_t) classification = (HASHTRAY(value_t)) (-1);
    bool known = USE_HASHTRAY && HASHTRAY(contains)(tbl, hinfo->id);

    if (known) {
      classification = HASHTRAY(lookup)(tbl, hinfo->id, NULL);
      sinfo->host_known += 1;

      switch (classification) {
      case BAD_HOST:
        #if defined(SHOW_PROGRESS)
        printf("!");
        fflush(stdout);
        #endif
        if (hinfo->is_good) {
          sinfo->host_classified_incorrect += 1;
        } else {
          sinfo->host_classified_correct += 1;
        }
        break;
      case GOOD_HOST:
        #if defined(SHOW_PROGRESS)
        printf(".");
        fflush(stdout);
        #endif
        if (!hinfo->is_good) {
          delay = (uint32_t) MAX_STALL;
          sinfo->host_classified_incorrect += 1;
        } else {
          sinfo->host_classified_correct += 1;
        }
        break;
      default:
        assert(false);
      }
    } else {
      sinfo->host_unknown += 1;

      if (!hinfo->is_good) {
        #if defined(SHOW_PROGRESS)
        printf("-");
        fflush(stdout);
        #endif
        classification = BAD_HOST;
        delay = (uint32_t) HASHTRAY(rand_range)(MIN_STALL, MAX_STALL);
      } else {
        #if defined(SHOW_PROGRESS)
        printf("+");
        fflush(stdout);
        #endif

        #if !defined(PERFECT_GOOD)
        delay = (uint32_t) HASHTRAY(rand_range)(0, MIN_STALL);
        #endif
        classification = GOOD_HOST;
      }

      if (USE_HASHTRAY) {
        #if !defined(USE_PERFECT_CLASSIFIER)
        if (delay > sinfo->avg_duration + DELAY_TOLERANCE) {
          classification = BAD_HOST;
          if (hinfo->is_good) {
            sinfo->host_classified_incorrect += 1;
          } else {
            sinfo->host_classified_correct += 1;
          }
        } else {
          classification = GOOD_HOST;
          if (hinfo->is_good) {
            sinfo->host_classified_correct += 1;
          } else {
            sinfo->host_classified_incorrect += 1;
          }
        }
        #endif
        HASHTRAY(insert)(tbl, hinfo->id, classification, NULL, NULL);
      }
    }

    sinfo->tot_duration += delay;
    sinfo->avg_duration = sinfo->tot_duration / sinfo->num_connections;
    #if defined(REALLY_SLEEP)
    sleep(delay);
    #endif

    lock_host(hinfo);
    hinfo->current_num_connections -= 1;
    assert(hinfo->current_num_connections >= 0);
    unlock_host(hinfo);
  }

  return NULL;
}

int main(int argc, char *argv[]) {
  bool dump_parameters = false;

  int32_t choice = 0;
  while ((choice = getopt(argc, argv, "dg:h:n:pu:v:")) != -1) {
    switch (choice) {
    case 'd':
      dump_parameters = true;
      break;
    case 'g':
      PERCENTAGE_GOOD_CONNECTION = (int32_t) strtol(optarg, NULL, 10);
      break;
    case 'h':
      PERCENTAGE_GOOD_HOSTS = (int32_t) strtol(optarg, NULL, 10);
      break;
    case 'n':
      NUM_HOSTS = (int32_t) strtol(optarg, NULL, 10);
      assert(NUM_HOSTS <= MAX_NUM_HOSTS);
      break;
    case 'p':
      USE_HASHTRAY = true;
      break;
    case 'u':
      MIN_STALL = (int32_t) strtol(optarg, NULL, 10);
      break;
    case 'v':
      MAX_STALL = (int32_t) strtol(optarg, NULL, 10);
      break;
    default:
      fprintf(stderr, "Unrecognised option: %c\n", choice);
      exit(2);
    }
  }

  if (optind < argc) {
    fprintf(stderr, "Unrecognised options: ");
    while (optind < argc) {
      fprintf(stderr, "%s ", argv[optind]);
      optind += 1;
    }
    fprintf(stderr, "\n");
    exit(2);
  }

  assert(MIN_STALL <= MAX_STALL);

  if (dump_parameters) {
    #if defined(REALLY_SLEEP)
    printf("REALLY_SLEEP=yes\n");
    #else
    printf("REALLY_SLEEP=no\n");
    #endif

    #if defined(SHOW_PROGRESS)
    printf("SHOW_PROGRESS=yes\n");
    #else
    printf("SHOW_PROGRESS=no\n");
    #endif

    #if defined(VERBOSE)
    printf("VERBOSE=yes\n");
    #else
    printf("VERBOSE=no\n");
    #endif

    #if defined(PERFECT_GOOD)
    printf("PERFECT_GOOD=yes\n");
    #else
    printf("PERFECT_GOOD=no\n");
    #endif

    #if defined(USE_PERFECT_CLASSIFIER)
    printf("USE_PERFECT_CLASSIFIER=yes\n");
    #else
    printf("USE_PERFECT_CLASSIFIER=no\n");
    #endif

    #if defined(SIM_DURATION_IN_SECONDS)
    printf("SIM_DURATION_IN_SECONDS=yes\n");
    #else
    printf("SIM_DURATION_IN_SECONDS=no\n");
    #endif

    #if defined(SIM_DURATION_IN_CONNECTIONS)
    printf("SIM_DURATION_IN_CONNECTIONS=yes\n");
    #else
    printf("SIM_DURATION_IN_CONNECTIONS=no\n");
    #endif
    printf("USE_HASHTRAY=%d\n", USE_HASHTRAY);
    printf("NUM_SERVERS=%d\n", NUM_SERVERS);
    printf("SIM_DURATION_SECS=%d\n", SIM_DURATION_SECS);
    printf("SIM_DURATION_CONNS=%d\n", SIM_DURATION_CONNS);
    printf("MAX_NUM_HOSTS=%d\n", MAX_NUM_HOSTS);
    printf("NUM_HOSTS=%d\n", NUM_HOSTS);
    printf("PERCENTAGE_GOOD_HOSTS=%d\n", PERCENTAGE_GOOD_HOSTS);
    printf("PERCENTAGE_GOOD_CONNECTION=%d\n", PERCENTAGE_GOOD_CONNECTION);
    printf("MAX_SLEEP=%d\n", MAX_SLEEP);
    printf("MIN_STALL=%d\n", MIN_STALL);
    printf("MAX_STALL=%d\n", MAX_STALL);
    printf("MAX_CONNS=%d\n", MAX_CONNS);
    printf("DELAY_TOLERANCE=%d\n", DELAY_TOLERANCE);
    printf("MAX_ITERATIONS=%d\n", MAX_ITERATIONS);
    printf("RAND_SEED=%d\n", RAND_SEED);
    exit(0);
  }

  atexit(exit_handler);
  init_signals();
  srand(RAND_SEED);

  #if defined(SIM_DURATION_IN_SECONDS)
  alarm(SIM_DURATION_SECS);
  #endif

  bool error = false;

  #if defined(SIM_DURATION_IN_CONNECTIONS)
  error = pthread_mutex_init(&connections_countdown_lock, NULL);
  assert(!error);
  #endif

  generate_hosts();
  #if defined(VERBOSE)
  print_host_info(false);
  #endif

  tbl = HASHTRAY(create_table)();

  for (int32_t i = 0; i < NUM_SERVERS; i++) {
    server_info[i].seed = HASHTRAY(rand_range)(0, INT_MAX);
    server_info[i].shutdown = false;
    server_info[i].idx = (uint8_t) i;
    int32_t create_error = pthread_create(
      &(tid[i]),
      NULL,
      &server_main,
      (void *) &(server_info[i])
    );
    if (create_error != 0) {
      fprintf(stderr, "pthread_create: %s\n", strerror(create_error));
      exit(1);
    }
  }

  for (int32_t i = 0; i < NUM_SERVERS; i++) {
    pthread_join(tid[i], NULL);
  }

  HASHTRAY(destroy_table)(tbl);
  shutdown_hosts();

  #if defined(SIM_DURATION_IN_CONNECTIONS)
  error = pthread_mutex_destroy(&connections_countdown_lock);
  assert(!error);
  #endif

  print_server_info();

  #if defined(VERBOSE)
  printf("done\n");
  #endif
  return 0;
}
