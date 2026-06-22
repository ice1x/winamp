/*
 * test_nxonce.c - osx-arm64 port of nx/nxonce.h
 *
 * Verifies NXOnce runs the init function exactly once even when many threads
 * race on the same nx_once_t, and that callers observe the writes the init
 * function made (acquire/release ordering, which the naive Win32 flag version
 * explicitly warns is unsafe on weakly-ordered CPUs like ARM) (-arch arm64).
 */
#include "nx/nxonce.h"
#include <pthread.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL: %s (line %d)\n", #cond, __LINE__);                       \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

static int init_calls;
static int payload; /* a value published by the init function */

static int the_init(nx_once_t once, void *param, void **out) {
  (void)once;
  (void)param;
  (void)out;
  ++init_calls;  /* guarded by NXOnce's own mutex, so plain ++ is fine */
  payload = 0xC0FFEE;
  return 0;
}

#define NTHREADS 16
static nx_once_t g_once;
static volatile int g_observed_payload_mismatch;

static void *racer(void *arg) {
  (void)arg;
  NXOnce(g_once, the_init, NULL);
  /* after NXOnce returns, the payload write must be visible */
  if (payload != 0xC0FFEE)
    g_observed_payload_mismatch = 1;
  return NULL;
}

int main(void) {
  printf("test_nxonce (osx-arm64)\n");

  nx_once_value_t once;
  g_once = &once;
  NXOnceInit(g_once);
  init_calls = 0;
  payload = 0;
  g_observed_payload_mismatch = 0;

  pthread_t t[NTHREADS];
  for (int i = 0; i < NTHREADS; ++i)
    pthread_create(&t[i], NULL, racer, NULL);
  for (int i = 0; i < NTHREADS; ++i)
    pthread_join(t[i], NULL);

  CHECK(init_calls == 1);
  CHECK(payload == 0xC0FFEE);
  CHECK(g_observed_payload_mismatch == 0);

  /* a subsequent call is a no-op */
  NXOnce(g_once, the_init, NULL);
  CHECK(init_calls == 1);

  if (failures == 0) {
    printf("  OK\n");
    return 0;
  }
  printf("  %d failure(s)\n", failures);
  return 1;
}
