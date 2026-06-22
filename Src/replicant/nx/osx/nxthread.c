/*
 macOS implementation of the nx thread primitives. See osx/nxthread.h.
*/
#include "nxthread.h"
#include "foundation/error.h"
#include <sched.h>
#include <stdlib.h>

/* The handle owns the pthread plus the captured user function, its parameter
   and its return value. It is heap-allocated so the handle stays valid for the
   lifetime of the thread regardless of the caller's stack. */
struct nx_thread_struct_t {
  pthread_t thread;
  nx_thread_func_t func;
  nx_thread_parameter_t param;
  nx_thread_return_t retval;
};

/* Adapts the Win32-style integral-returning thread function to pthread's
   void *(*)(void *) signature, stashing the return value in the handle. */
static void *nx_thread_trampoline(void *arg) {
  nx_thread_t t = (nx_thread_t)arg;
  t->retval = t->func(t->param);
  return NULL;
}

int NXThreadCreate(nx_thread_t *thread, nx_thread_func_t thread_function,
                   nx_thread_parameter_t parameter) {
  if (!thread || !thread_function)
    return NErr_NullPointer;

  nx_thread_t t = (nx_thread_t)calloc(1, sizeof(*t));
  if (!t)
    return NErr_OutOfMemory;

  t->func = thread_function;
  t->param = parameter;

  if (pthread_create(&t->thread, NULL, nx_thread_trampoline, t) != 0) {
    free(t);
    return NErr_FailedCreate;
  }

  *thread = t;
  return NErr_Success;
}

int NXThreadJoin(nx_thread_t t, nx_thread_return_t *retval) {
  if (!t)
    return NErr_NullPointer;

  pthread_join(t->thread, NULL);
  if (retval)
    *retval = t->retval;

  free(t);
  return NErr_Success;
}

int NXThreadCurrentSetPriority(int priority) {
  /* Best-effort: macOS has no direct THREAD_PRIORITY_* analogue, so map the
     request onto the current scheduling policy's priority range. A failure to
     raise priority is non-fatal (the player still runs), matching the Win32
     wrapper which ignores SetThreadPriority's return value. */
  int policy;
  struct sched_param param;
  pthread_t self = pthread_self();

  if (pthread_getschedparam(self, &policy, &param) == 0) {
    int prio_max = sched_get_priority_max(policy);
    int prio_min = sched_get_priority_min(policy);
    if (prio_max >= 0 && prio_min >= 0) {
      if (priority >= NX_THREAD_PRIORITY_PLAYBACK)
        param.sched_priority = prio_max;
      else if (priority <= 0)
        param.sched_priority = (prio_min + prio_max) / 2;
      else
        param.sched_priority =
            prio_min + (prio_max - prio_min) * 3 / 4;
      pthread_setschedparam(self, policy, &param);
    }
  }
  return NErr_Success;
}
