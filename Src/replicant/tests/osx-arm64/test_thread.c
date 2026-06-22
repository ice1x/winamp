/*
 * test_thread.c - osx-arm64 port of nx/nxthread.h
 *
 * Verifies NXThreadCreate spawns a thread that runs the supplied function with
 * its parameter, and NXThreadJoin blocks until completion and returns the
 * function's return value. The Win32 reference wraps CreateThread/
 * WaitForSingleObject; the POSIX port wraps pthread_create/pthread_join with a
 * trampoline that captures the return value (-arch arm64).
 */
#include "nx/nxthread.h"
#include "foundation/error.h"
#include <stdio.h>

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL: %s (line %d)\n", #cond, __LINE__);                       \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

/* a worker that squares the integer pointed to by its parameter and returns it
   through the thread return value. */
static nx_thread_return_t NXTHREADCALL square_worker(nx_thread_parameter_t p) {
  int n = *(int *)p;
  return (nx_thread_return_t)(n * n);
}

#define NTHREADS 8

int main(void) {
  printf("test_thread (osx-arm64)\n");

  /* single thread: parameter is passed through, return value is captured */
  int value = 7;
  nx_thread_t t = NULL;
  CHECK(NXThreadCreate(&t, square_worker, &value) == NErr_Success);
  CHECK(t != NULL);
  nx_thread_return_t ret = 0;
  CHECK(NXThreadJoin(t, &ret) == NErr_Success);
  CHECK(ret == 49);

  /* joining a NULL thread is a guarded no-op, not a crash */
  CHECK(NXThreadJoin(NULL, NULL) == NErr_NullPointer);

  /* many threads run concurrently, each with its own parameter */
  int inputs[NTHREADS];
  nx_thread_t threads[NTHREADS];
  for (int i = 0; i < NTHREADS; ++i) {
    inputs[i] = i + 1;
    CHECK(NXThreadCreate(&threads[i], square_worker, &inputs[i]) ==
          NErr_Success);
  }
  for (int i = 0; i < NTHREADS; ++i) {
    nx_thread_return_t r = 0;
    CHECK(NXThreadJoin(threads[i], &r) == NErr_Success);
    CHECK(r == (nx_thread_return_t)(inputs[i] * inputs[i]));
  }

  /* setting the current thread's priority must not fail */
  CHECK(NXThreadCurrentSetPriority(NX_THREAD_PRIORITY_PLAYBACK) == NErr_Success);

  if (failures == 0) {
    printf("  OK\n");
    return 0;
  }
  printf("  %d failure(s)\n", failures);
  return 1;
}
