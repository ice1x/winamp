/*

 macOS / Apple Silicon (arm64) implementation

 Counterpart to win-amd64/LockFreeLIFO.h, which maps straight onto the Win32
 SLIST (InterlockedPushEntrySList / ...Pop) intrinsics. arm64 has no equivalent
 ABA-safe single-instruction list primitive, so this is a small mutex-guarded
 intrusive stack. It honours the exact same lifo_* contract (and the documented
 "order is not strictly guaranteed across threads" caveat).

 NOTE: this is a correct, blocking fallback - NOT yet lock-free. A genuinely
 lock-free arm64 version (tagged-pointer CAS via the 128-bit LSE `casp`, or a
 hazard-pointer scheme) is tracked as future optimisation work (roadmap 00045).

*/

#pragma once

#include "nu/queue_node.h"
#include <pthread.h>
#include <stdlib.h>

#ifdef __cplusplus
#define NU_LIFO_INLINE inline
extern "C" {
#else
#define NU_LIFO_INLINE static inline
#endif

/* 16-byte alignment matches MEMORY_ALLOCATION_ALIGNMENT on the Win32 side and
   keeps payloads suitably aligned for any type the node may precede. */
#define NU_LIFO_ALIGNMENT 16

typedef struct lifo_s {
  queue_node_t *head;
  pthread_mutex_t lock;
} lifo_t;

/* use this to allocate an object that will go into this list */
NU_LIFO_INLINE queue_node_t *lifo_malloc(size_t bytes) {
  void *p = NULL;
  if (posix_memalign(&p, NU_LIFO_ALIGNMENT, bytes) != 0)
    return NULL;
  return (queue_node_t *)p;
}

NU_LIFO_INLINE void lifo_free(queue_node_t *ptr) { free(ptr); }

NU_LIFO_INLINE lifo_t *lifo_create(void) {
  void *p = NULL;
  if (posix_memalign(&p, NU_LIFO_ALIGNMENT, sizeof(lifo_t)) != 0)
    return NULL;
  return (lifo_t *)p;
}

NU_LIFO_INLINE void lifo_destroy(lifo_t *lifo) {
  if (!lifo)
    return;
  pthread_mutex_destroy(&lifo->lock);
  free(lifo);
}

NU_LIFO_INLINE void lifo_init(lifo_t *lifo) {
  lifo->head = NULL;
  pthread_mutex_init(&lifo->lock, NULL);
}

NU_LIFO_INLINE void lifo_push(lifo_t *lifo, queue_node_t *cl) {
  pthread_mutex_lock(&lifo->lock);
  cl->Next = lifo->head;
  lifo->head = cl;
  pthread_mutex_unlock(&lifo->lock);
}

NU_LIFO_INLINE queue_node_t *lifo_pop(lifo_t *lifo) {
  pthread_mutex_lock(&lifo->lock);
  queue_node_t *node = lifo->head;
  if (node)
    lifo->head = node->Next;
  pthread_mutex_unlock(&lifo->lock);
  return node;
}

#ifdef __cplusplus
}
#endif
