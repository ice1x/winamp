/*

 macOS / Apple Silicon (arm64) implementation of NXOnce.

 Counterpart to win/nxonce.h. The Win32 version guards a plain `int status`
 flag with a CRITICAL_SECTION and explicitly warns that the bare flag check is
 only safe because x86 has (near) total store ordering - it is NOT safe on a
 weakly-ordered CPU like arm64. This implementation therefore uses acquire/
 release atomics on `status` so a caller that sees status==1 is guaranteed to
 also see every write the init function made.

*/

#pragma once
#include "../nxapi.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nx_once_s {
  volatile int status;
  pthread_mutex_t mutex;
} nx_once_value_t, *nx_once_t;

#define NX_ONCE_API

NX_API void NXOnce(nx_once_t once,
                   int(NX_ONCE_API *init_fn)(nx_once_t, void *, void **),
                   void *param);
NX_API void NXOnceInit(nx_once_t once);

#ifdef __cplusplus
}
#endif
