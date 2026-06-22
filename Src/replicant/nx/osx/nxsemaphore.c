/*
 macOS implementation of the nx counting semaphore. See osx/nxsemaphore.h.
*/
#include "nxsemaphore.h"
#include "foundation/error.h"

int NXSemaphoreCreate(nx_semaphore_t *sem) {
  if (!sem)
    return NErr_NullPointer;

  /* count starts at 0, matching CreateSemaphore(0, 0, LONG_MAX, 0). */
  *sem = dispatch_semaphore_create(0);
  if (!*sem)
    return NErr_FailedCreate;
  return NErr_Success;
}

int NXSemaphoreRelease(nx_semaphore_t sem) {
  if (!sem)
    return NErr_NullPointer;

  dispatch_semaphore_signal(sem);
  return NErr_Success;
}

int NXSemaphoreWait(nx_semaphore_t sem) {
  if (!sem)
    return NErr_NullPointer;

  dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
  return NErr_Success;
}

void NXSemaphoreClose(nx_semaphore_t sem) {
  if (!sem)
    return;

  /* dispatch objects are reference-counted; release our reference. Compiled as
     C (no ARC), so the explicit dispatch_release is required. */
  dispatch_release(sem);
}
