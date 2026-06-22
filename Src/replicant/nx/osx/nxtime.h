/*

 macOS implementation of the nx time typedefs.

 Counterpart to win/nxtime.h: a Unix-epoch timestamp type shared by the
 cross-platform core. Kept header-only to match the Win32 version (the
 dispatcher nx/nxtime.h routes __APPLE__ here).

*/

#pragma once
#include "../../foundation/types.h"
#include "../../nx/nxapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t nx_time_unix_64_t;

#ifdef __cplusplus
}
#endif
