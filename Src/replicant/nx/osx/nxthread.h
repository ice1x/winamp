/*

 macOS implementation of the nx thread primitives.

 Counterpart to win/nxthread.h (CreateThread/WaitForSingleObject). The POSIX
 port wraps pthread_create/pthread_join. Because a Win32 thread function returns
 an integral nx_thread_return_t while pthread expects void *(*)(void *), the
 created thread runs through a small trampoline (see nxthread.c) that captures
 the return value so NXThreadJoin can hand it back.

*/

#pragma once
#include "nx/nxapi.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nx_thread_struct_t *nx_thread_t;
typedef unsigned int nx_thread_return_t;
typedef void *nx_thread_parameter_t;
#define NXTHREADCALL
typedef nx_thread_return_t(NXTHREADCALL *nx_thread_func_t)(
    nx_thread_parameter_t parameter);

// TODO: add parameters for things like stack size
NX_API int NXThreadCreate(nx_thread_t *thread, nx_thread_func_t thread_function,
                          nx_thread_parameter_t parameter);
NX_API int NXThreadJoin(nx_thread_t t, nx_thread_return_t *retval);

enum {
  /* mirrors THREAD_PRIORITY_HIGHEST on Win32; mapped to the policy maximum. */
  NX_THREAD_PRIORITY_PLAYBACK = 2,
};
// sets priority of current thread
NX_API int NXThreadCurrentSetPriority(int priority);

#ifdef __cplusplus
}
#endif
