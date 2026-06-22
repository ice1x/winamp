/*

 macOS implementation of the nx public-API decoration macros.

 Counterpart to win/nxapi.h: on Win32 NX_API is __declspec(dll{export,import});
 on macOS we use GCC/clang symbol-visibility attributes instead.

*/

#pragma once
#include "../../foundation/guid.h"

#ifdef NX_EXPORTS
#define NX_API __attribute__((visibility("default")))
#else
#define NX_API
#endif

/* increment this any time that the NX API changes in a non-backwards-compatible
   way (preferably rarely) */
static const int nx_api_version = 1;

// {E7079A4B-BBB3-441F-ADCD-E0F1FE276EE3}
static const GUID nx_platform_guid = {
    0xe7079a4b,
    0xbbb3,
    0x441f,
    {0xad, 0xcd, 0xe0, 0xf1, 0xfe, 0x27, 0x6e, 0xe3}};
