/*
 macOS / Apple Silicon (arm64) implementation of NXOnce.
 See osx-arm64/nxonce.h for the rationale behind the acquire/release ordering.
*/

#include "nxonce.h"

void NXOnce(nx_once_t once,
            int(NX_ONCE_API *init_fn)(nx_once_t, void *, void **),
            void *param) {
  /* fast path: an acquire load pairs with the release store below, so once we
     observe status==1 the init function's writes are guaranteed visible. */
  if (__atomic_load_n(&once->status, __ATOMIC_ACQUIRE))
    return;

  pthread_mutex_lock(&once->mutex);
  if (__atomic_load_n(&once->status, __ATOMIC_ACQUIRE)) {
    pthread_mutex_unlock(&once->mutex);
    return;
  }

  init_fn(once, param, 0);

  /* release: publish every write init_fn made before status flips to 1. */
  __atomic_store_n(&once->status, 1, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&once->mutex);
}

void NXOnceInit(nx_once_t once) {
  once->status = 0;
  pthread_mutex_init(&once->mutex, NULL);
}
