/*

 macOS implementation of the nx sleep primitives.

 Counterpart to win/nxsleep.h. The Win32 version is built on Sleep(); this one
 maps onto POSIX nanosleep()/sched_yield(). The public surface is identical so
 callers in the cross-platform core need no changes.

*/

#pragma once
#include "nx/nxapi.h"

#ifdef __cplusplus
extern "C" {
#endif

NX_API int NXSleep(unsigned int milliseconds);
NX_API int NXSleepYield(void);

#ifdef __cplusplus
}
#endif
