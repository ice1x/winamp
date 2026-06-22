/*

 macOS / Apple Silicon (arm64) implementation

 Counterpart to win-amd64/atomics.h. arm64 is weakly ordered, so we use the
 C/C++11 atomic builtins (__atomic_*) with explicit memory orders rather than
 the implicit full barriers that the x86/Win32 Interlocked* intrinsics carry.

 Semantics deliberately match the Win32 reference:
   - inc / dec / dec_release return the NEW value (like InterlockedIncrement)
   - add / sub             return the PREVIOUS value (like InterlockedExchangeAdd)
   - swap_pointer          returns the previous pointer
   - cmpxchg_pointer       returns 1 if the swap happened, else 0

*/

#pragma once
#include "../../foundation/types.h"

#ifdef __cplusplus
#define NX_ATOMIC_INLINE inline
#else
#define NX_ATOMIC_INLINE static inline
#endif

NX_ATOMIC_INLINE size_t nx_atomic_inc(volatile size_t *addr) {
  return __atomic_add_fetch(addr, 1, __ATOMIC_SEQ_CST);
}

NX_ATOMIC_INLINE size_t nx_atomic_dec(volatile size_t *addr) {
  return __atomic_sub_fetch(addr, 1, __ATOMIC_SEQ_CST);
}

/* release semantics: all prior writes are visible before the count drops.
   used for reference-count release, where the final decrementer must see
   every other thread's writes. */
NX_ATOMIC_INLINE size_t nx_atomic_dec_release(volatile size_t *addr) {
  return __atomic_sub_fetch(addr, 1, __ATOMIC_ACQ_REL);
}

NX_ATOMIC_INLINE void nx_atomic_write(size_t value, volatile size_t *addr) {
  __atomic_store_n(addr, value, __ATOMIC_SEQ_CST);
}

NX_ATOMIC_INLINE void nx_atomic_write_pointer(void *value,
                                              void *volatile *addr) {
  __atomic_store_n(addr, value, __ATOMIC_SEQ_CST);
}

NX_ATOMIC_INLINE size_t nx_atomic_add(size_t value, volatile size_t *addr) {
  return __atomic_fetch_add(addr, value, __ATOMIC_SEQ_CST);
}

NX_ATOMIC_INLINE size_t nx_atomic_sub(size_t value, volatile size_t *addr) {
  return __atomic_fetch_sub(addr, value, __ATOMIC_SEQ_CST);
}

NX_ATOMIC_INLINE void *nx_atomic_swap_pointer(void *value,
                                              void *volatile *addr) {
  return __atomic_exchange_n(addr, value, __ATOMIC_SEQ_CST);
}

NX_ATOMIC_INLINE int nx_atomic_cmpxchg_pointer(void *oldvalue, void *newvalue,
                                               void *volatile *addr) {
  /* weak=false: a single, definite compare-and-swap attempt */
  return __atomic_compare_exchange_n(addr, &oldvalue, newvalue, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
             ? 1
             : 0;
}
