/*
 macOS implementation of the nx condition variable. See osx/nxcondition.h.
*/
#include "nxcondition.h"
#include "foundation/error.h"
#include <errno.h>
#include <sys/time.h>

int NXConditionInitialize(nx_condition_t condition) {
  if (condition == 0)
    return NErr_NullPointer;

  pthread_mutex_init(&condition->mutex, NULL);
  pthread_cond_init(&condition->condition, NULL);
  return NErr_Success;
}

int NXConditionDestroy(nx_condition_t condition) {
  if (condition == 0)
    return NErr_NullPointer;

  pthread_cond_destroy(&condition->condition);
  pthread_mutex_destroy(&condition->mutex);
  return NErr_Success;
}

int NXConditionLock(nx_condition_t condition) {
  if (condition == 0)
    return NErr_NullPointer;

  pthread_mutex_lock(&condition->mutex);
  return NErr_Success;
}

int NXConditionUnlock(nx_condition_t condition) {
  if (condition == 0)
    return NErr_NullPointer;

  pthread_mutex_unlock(&condition->mutex);
  return NErr_Success;
}

int NXConditionWait(nx_condition_t condition) {
  if (condition == 0)
    return NErr_NullPointer;

  /* the caller must already hold the lock (matches SleepConditionVariableCS). */
  pthread_cond_wait(&condition->condition, &condition->mutex);
  return NErr_Success;
}

int NXConditionTimedWait(nx_condition_t condition, unsigned int milliseconds) {
  if (condition == 0)
    return NErr_NullPointer;

  /* pthread_cond_timedwait takes an absolute deadline against the realtime
     clock; build it from now + the requested relative timeout. */
  struct timeval now;
  gettimeofday(&now, NULL);

  long long ns = (long long)now.tv_usec * 1000LL +
                 (long long)(milliseconds % 1000u) * 1000000LL;
  struct timespec deadline;
  deadline.tv_sec = now.tv_sec + milliseconds / 1000u + (time_t)(ns / 1000000000LL);
  deadline.tv_nsec = (long)(ns % 1000000000LL);

  int rc = pthread_cond_timedwait(&condition->condition, &condition->mutex,
                                  &deadline);
  /* a timeout is a normal outcome, not an error, just like the Win32 version
     which always returns NErr_Success regardless of WAIT_TIMEOUT. */
  if (rc != 0 && rc != ETIMEDOUT)
    return NErr_Error;
  return NErr_Success;
}

int NXConditionSignal(nx_condition_t condition) {
  if (condition == 0)
    return NErr_NullPointer;

  pthread_cond_signal(&condition->condition);
  return NErr_Success;
}
