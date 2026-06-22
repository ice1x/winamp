/*

 macOS implementation of the nx counting semaphore.

 Counterpart to win/nxsemaphore.h (a HANDLE from CreateSemaphore). macOS
 sem_init()/unnamed POSIX semaphores are a non-functional stub (return ENOSYS),
 so this port is built on a Grand Central Dispatch dispatch_semaphore_t, which
 provides the same counting-semaphore semantics: created with a count of 0,
 Release raises the count, Wait lowers it / blocks.

*/

#pragma once
#include "nx/nxapi.h"
#include <dispatch/dispatch.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef dispatch_semaphore_t nx_semaphore_t;

NX_API int NXSemaphoreCreate(nx_semaphore_t *sem);
NX_API int NXSemaphoreRelease(nx_semaphore_t sem);
NX_API int NXSemaphoreWait(nx_semaphore_t sem);
NX_API void NXSemaphoreClose(nx_semaphore_t sem);

#ifdef __cplusplus
}
#endif
