/*
 * test_semaphore.c - osx-arm64 port of nx/nxsemaphore.h
 *
 * Verifies the counting-semaphore semantics of the Win32 reference (a
 * semaphore created with count 0; Release increments, Wait decrements/blocks).
 * macOS sem_init() is a non-functional stub, so the port is built on a GCD
 * dispatch_semaphore_t instead (-arch arm64).
 */
#include "nx/nxsemaphore.h"
#include "foundation/error.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL: %s (line %d)\n", #cond, __LINE__);                       \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

/* Producer/consumer integrity: one thread releases N times, the main thread
   waits N times and must observe exactly N completions without blocking
   forever. */
#define N 1000

static nx_semaphore_t g_sem;

static void *producer(void *arg) {
  (void)arg;
  for (int i = 0; i < N; ++i)
    NXSemaphoreRelease(g_sem);
  return NULL;
}

/* signalled by a worker after it parks, to prove Wait actually blocks until a
   Release arrives rather than returning spuriously. */
static nx_semaphore_t g_handoff;
static volatile int g_worker_ran;

static void *late_signaller(void *arg) {
  (void)arg;
  usleep(50 * 1000); /* let the main thread reach NXSemaphoreWait first */
  g_worker_ran = 1;
  NXSemaphoreRelease(g_handoff);
  return NULL;
}

int main(void) {
  printf("test_semaphore (osx-arm64)\n");

  CHECK(NXSemaphoreCreate(&g_sem) == NErr_Success);
  CHECK(g_sem != NULL);

  /* count starts at 0: a Release then Wait must pass straight through */
  NXSemaphoreRelease(g_sem);
  CHECK(NXSemaphoreWait(g_sem) == NErr_Success);

  /* concurrent producer releases N permits; we consume all N */
  pthread_t prod;
  pthread_create(&prod, NULL, producer, NULL);
  for (int i = 0; i < N; ++i)
    CHECK(NXSemaphoreWait(g_sem) == NErr_Success);
  pthread_join(prod, NULL);

  /* Wait must genuinely block until a Release happens */
  CHECK(NXSemaphoreCreate(&g_handoff) == NErr_Success);
  g_worker_ran = 0;
  pthread_t worker;
  pthread_create(&worker, NULL, late_signaller, NULL);
  CHECK(NXSemaphoreWait(g_handoff) == NErr_Success);
  CHECK(g_worker_ran == 1); /* we could only proceed after the worker released */
  pthread_join(worker, NULL);

  NXSemaphoreClose(g_sem);
  NXSemaphoreClose(g_handoff);

  if (failures == 0) {
    printf("  OK\n");
    return 0;
  }
  printf("  %d failure(s)\n", failures);
  return 1;
}
