/*
 * test_sleep.c - osx-arm64 port of nx/nxsleep.h
 *
 * Verifies NXSleep blocks for at least (approximately) the requested duration
 * and that both entry points report NErr_Success. The Win32 reference maps
 * NXSleep onto Sleep(ms) and NXSleepYield onto Sleep(0); the POSIX port maps
 * them onto nanosleep()/sched_yield() (-arch arm64).
 */
#include "nx/nxsleep.h"
#include "foundation/error.h"
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

static double elapsed_ms(struct timespec a, struct timespec b) {
  return (b.tv_sec - a.tv_sec) * 1000.0 +
         (b.tv_nsec - a.tv_nsec) / 1000000.0;
}

int main(void) {
  printf("test_sleep (osx-arm64)\n");

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  CHECK(NXSleep(50) == NErr_Success);
  clock_gettime(CLOCK_MONOTONIC, &end);

  double ms = elapsed_ms(start, end);
  /* must have slept at least ~the requested time (allow a little slack for
     timer granularity); the upper bound only guards against a pathological
     hang, not scheduler jitter. */
  CHECK(ms >= 45.0);
  CHECK(ms < 5000.0);

  /* yield must not hang and must report success */
  CHECK(NXSleepYield() == NErr_Success);

  /* a zero-millisecond sleep behaves like a yield: returns promptly */
  clock_gettime(CLOCK_MONOTONIC, &start);
  CHECK(NXSleep(0) == NErr_Success);
  clock_gettime(CLOCK_MONOTONIC, &end);
  CHECK(elapsed_ms(start, end) < 1000.0);

  if (failures == 0) {
    printf("  OK\n");
    return 0;
  }
  printf("  %d failure(s)\n", failures);
  return 1;
}
