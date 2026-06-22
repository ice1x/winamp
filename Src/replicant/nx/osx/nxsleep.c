/*
 macOS implementation of the nx sleep primitives. See osx/nxsleep.h.
*/
#include "nxsleep.h"
#include "foundation/error.h"
#include <errno.h>
#include <sched.h>
#include <time.h>

int NXSleep(unsigned int milliseconds) {
  /* nanosleep is interruptible by signals; resume with the remaining time so a
     spurious signal does not cut the requested duration short. */
  struct timespec req;
  req.tv_sec = milliseconds / 1000u;
  req.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;

  struct timespec rem;
  while (nanosleep(&req, &rem) == -1 && errno == EINTR)
    req = rem;

  return NErr_Success;
}

int NXSleepYield(void) {
  /* matches Win32 Sleep(0): relinquish the rest of the time slice. */
  sched_yield();
  return NErr_Success;
}
