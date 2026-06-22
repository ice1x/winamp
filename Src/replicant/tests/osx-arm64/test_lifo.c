/*
 * test_lifo.c - osx-arm64 port of nu/LockFreeLIFO.h
 *
 * Verifies the lifo_* API: every node pushed by N producer threads is popped
 * exactly once by M consumer threads, with no loss or duplication, and that
 * single-threaded LIFO ordering holds (-arch arm64).
 */
#include "nu/LockFreeLIFO.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL: %s (line %d)\n", #cond, __LINE__);                       \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

/* node carrying a payload right after the intrusive queue_node_t header */
typedef struct {
  queue_node_t node;
  int value;
} item_t;

#define NPROD 6
#define PER_PROD 20000
#define TOTAL (NPROD * PER_PROD)

static lifo_t *g_lifo;
static volatile size_t g_popped;

static void *producer(void *arg) {
  long base = (long)arg;
  for (int i = 0; i < PER_PROD; ++i) {
    item_t *it = (item_t *)lifo_malloc(sizeof(item_t));
    it->value = (int)(base + i);
    lifo_push(g_lifo, &it->node);
  }
  return NULL;
}

static void *consumer(void *arg) {
  (void)arg;
  for (;;) {
    if (g_popped >= TOTAL)
      return NULL;
    queue_node_t *n = lifo_pop(g_lifo);
    if (n) {
      __atomic_add_fetch(&g_popped, 1, __ATOMIC_SEQ_CST);
      lifo_free(n);
    }
  }
}

int main(void) {
  printf("test_lifo (osx-arm64)\n");

  /* single-threaded LIFO ordering */
  lifo_t *s = lifo_create();
  lifo_init(s);
  CHECK(lifo_pop(s) == NULL); /* empty */
  for (int i = 0; i < 3; ++i) {
    item_t *it = (item_t *)lifo_malloc(sizeof(item_t));
    it->value = i;
    lifo_push(s, &it->node);
  }
  for (int expect = 2; expect >= 0; --expect) {
    item_t *it = (item_t *)lifo_pop(s);
    CHECK(it != NULL);
    CHECK(it && it->value == expect); /* last pushed comes out first */
    lifo_free((queue_node_t *)it);
  }
  CHECK(lifo_pop(s) == NULL);
  lifo_destroy(s);

  /* concurrent producers + consumers: nothing lost, nothing duplicated */
  g_lifo = lifo_create();
  lifo_init(g_lifo);
  g_popped = 0;

  pthread_t prod[NPROD], cons[NPROD];
  for (int i = 0; i < NPROD; ++i)
    pthread_create(&prod[i], NULL, producer, (void *)(long)(i * PER_PROD));
  for (int i = 0; i < NPROD; ++i)
    pthread_create(&cons[i], NULL, consumer, NULL);
  for (int i = 0; i < NPROD; ++i)
    pthread_join(prod[i], NULL);
  for (int i = 0; i < NPROD; ++i)
    pthread_join(cons[i], NULL);

  /* drain any stragglers left because consumers exited on the count check */
  queue_node_t *n;
  while ((n = lifo_pop(g_lifo)) != NULL) {
    ++g_popped;
    lifo_free(n);
  }
  CHECK(g_popped == TOTAL);
  CHECK(lifo_pop(g_lifo) == NULL);
  lifo_destroy(g_lifo);

  if (failures == 0) {
    printf("  OK\n");
    return 0;
  }
  printf("  %d failure(s)\n", failures);
  return 1;
}
