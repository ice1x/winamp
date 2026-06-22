/*
 * test_atomics.c - osx-arm64 port of replicant foundation/atomics.h
 *
 * Verifies the nx_atomic_* primitives preserve the semantics of the Win32
 * (Interlocked*) reference implementation:
 *   - inc/dec/dec_release return the *new* value
 *   - add/sub return the *previous* value
 *   - swap_pointer returns the old pointer
 *   - cmpxchg_pointer swaps only on match and reports success as 1/0
 * and that they are genuinely atomic under contention (-arch arm64).
 */
#include "foundation/atomics.h"
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

#define NTHREADS 8
#define NITERS 100000

static volatile size_t shared_counter;

static void *hammer_inc(void *arg) {
  (void)arg;
  for (int i = 0; i < NITERS; ++i)
    nx_atomic_inc(&shared_counter);
  return NULL;
}

static void *hammer_add(void *arg) {
  (void)arg;
  for (int i = 0; i < NITERS; ++i)
    nx_atomic_add(2, &shared_counter);
  return NULL;
}

int main(void) {
  printf("test_atomics (osx-arm64)\n");

  /* single-threaded semantics */
  size_t v = 10;
  CHECK(nx_atomic_inc(&v) == 11); /* returns new value */
  CHECK(v == 11);
  CHECK(nx_atomic_dec(&v) == 10); /* returns new value */
  CHECK(v == 10);
  CHECK(nx_atomic_dec_release(&v) == 9);
  CHECK(v == 9);

  CHECK(nx_atomic_add(5, &v) == 9); /* returns previous value */
  CHECK(v == 14);
  CHECK(nx_atomic_sub(4, &v) == 14); /* returns previous value */
  CHECK(v == 10);

  nx_atomic_write(42, &v);
  CHECK(v == 42);

  /* pointer ops */
  int a = 1, b = 2;
  void *p = &a;
  nx_atomic_write_pointer(&b, &p);
  CHECK(p == &b);

  void *old = nx_atomic_swap_pointer(&a, &p);
  CHECK(old == &b);
  CHECK(p == &a);

  CHECK(nx_atomic_cmpxchg_pointer(&a, &b, &p) == 1); /* matches -> swap */
  CHECK(p == &b);
  CHECK(nx_atomic_cmpxchg_pointer(&a, &b, &p) == 0); /* no match -> no swap */
  CHECK(p == &b);

  /* concurrent atomicity: no lost updates */
  shared_counter = 0;
  pthread_t t[NTHREADS];
  for (int i = 0; i < NTHREADS; ++i)
    pthread_create(&t[i], NULL, (i & 1) ? hammer_add : hammer_inc, NULL);
  for (int i = 0; i < NTHREADS; ++i)
    pthread_join(t[i], NULL);
  /* half the threads do +1, half do +2 */
  size_t inc_threads = (NTHREADS + 1) / 2;
  size_t add_threads = NTHREADS / 2;
  size_t expected = (inc_threads * NITERS) + (add_threads * NITERS * 2);
  CHECK(shared_counter == expected);

  if (failures == 0) {
    printf("  OK\n");
    return 0;
  }
  printf("  %d failure(s)\n", failures);
  return 1;
}
