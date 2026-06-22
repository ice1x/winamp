/*
 * test_condition.c - osx-arm64 port of nx/nxcondition.h
 *
 * Verifies the condition-variable wrapper: Lock/Unlock guard a predicate,
 * Signal wakes a blocked Wait, and TimedWait returns (rather than hanging)
 * when no signal arrives. The Win32 reference uses CONDITION_VARIABLE +
 * CRITICAL_SECTION; the POSIX port uses pthread_cond_t + pthread_mutex_t
 * (-arch arm64).
 */
#include "nx/nxcondition.h"
#include "foundation/error.h"
#include <pthread.h>
#include <stdio.h>
#include <time.h>

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL: %s (line %d)\n", #cond, __LINE__);                       \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

static nx_condition_value_t g_cond;
static int g_ready;       /* predicate, guarded by the condition's mutex */

static void *waiter(void *arg) {
  (void)arg;
  NXConditionLock(&g_cond);
  while (!g_ready)
    NXConditionWait(&g_cond);
  NXConditionUnlock(&g_cond);
  return NULL;
}

static double elapsed_ms(struct timespec a, struct timespec b) {
  return (b.tv_sec - a.tv_sec) * 1000.0 +
         (b.tv_nsec - a.tv_nsec) / 1000000.0;
}

int main(void) {
  printf("test_condition (osx-arm64)\n");

  CHECK(NXConditionInitialize(&g_cond) == NErr_Success);
  CHECK(NXConditionInitialize(NULL) == NErr_NullPointer);

  /* Signal must wake a thread blocked in Wait, and the predicate write must be
     visible to it (the mutex provides the ordering). */
  g_ready = 0;
  pthread_t t;
  pthread_create(&t, NULL, waiter, NULL);

  NXConditionLock(&g_cond);
  g_ready = 1;
  NXConditionSignal(&g_cond);
  NXConditionUnlock(&g_cond);

  pthread_join(t, NULL); /* would hang forever if Signal did not wake Wait */

  /* TimedWait must time out and return instead of blocking forever when no
     signal is delivered. */
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  NXConditionLock(&g_cond);
  CHECK(NXConditionTimedWait(&g_cond, 100) == NErr_Success);
  NXConditionUnlock(&g_cond);
  clock_gettime(CLOCK_MONOTONIC, &end);
  double ms = elapsed_ms(start, end);
  CHECK(ms >= 90.0);   /* actually waited roughly the requested time */
  CHECK(ms < 5000.0);  /* but did not hang */

  CHECK(NXConditionDestroy(&g_cond) == NErr_Success);
  CHECK(NXConditionDestroy(NULL) == NErr_NullPointer);

  if (failures == 0) {
    printf("  OK\n");
    return 0;
  }
  printf("  %d failure(s)\n", failures);
  return 1;
}
