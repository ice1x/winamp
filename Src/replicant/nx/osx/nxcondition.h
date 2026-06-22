/*

 macOS implementation of the nx condition variable.

 Counterpart to win/nxcondition.h, which pairs a CONDITION_VARIABLE with a
 CRITICAL_SECTION. The POSIX port pairs a pthread_cond_t with a
 pthread_mutex_t; Lock/Unlock map onto the mutex and Wait/Signal onto the
 condition variable, preserving the same public API.

*/

#pragma once
#include "nx/nxapi.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nx_condition_struct_t {
  pthread_cond_t condition;
  pthread_mutex_t mutex;
} nx_condition_value_t, *nx_condition_t;

NX_API int NXConditionInitialize(nx_condition_t condition);
NX_API int NXConditionDestroy(nx_condition_t condition);
NX_API int NXConditionLock(nx_condition_t condition);
NX_API int NXConditionUnlock(nx_condition_t condition);
NX_API int NXConditionWait(nx_condition_t condition);
NX_API int NXConditionTimedWait(nx_condition_t condition,
                                unsigned int milliseconds);
NX_API int NXConditionSignal(nx_condition_t condition);

#ifdef __cplusplus
}
#endif
