/*

 macOS / Apple Silicon (arm64) implementation

 Counterpart to win-amd64/types.h. Unlike the Win32 header this pulls nothing
 from windows.h: the integer types come from <stdint.h> and the colour/helper
 aliases are pinned to fixed widths so packed-pixel code keeps working on a
 platform where `long` is 64-bit (it is 32-bit on Win64).

*/

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h> /* ssize_t, socklen_t come from the system here */
#include <wchar.h>

/* small signed/unsigned integer aliases */
typedef unsigned int UINT;
typedef signed int SINT;

typedef unsigned char UCHAR;
typedef signed char SCHAR;

/* packed pixel formats - pinned to fixed widths (the Win32 header uses
   `unsigned long`, which is 64-bit on macOS and would silently change the
   size of these). */
typedef uint32_t ARGB32;
typedef uint32_t RGB32;

typedef uint32_t ARGB24;
typedef uint32_t RGB24;

typedef uint16_t ARGB16;
typedef uint16_t RGB16;

typedef uint32_t FOURCC;

/* replicant treats these as UTF-16 code units (as on Win32, where wchar_t is
   16-bit). macOS wchar_t is 32-bit, so we pin them to a 16-bit unit; the
   string-layer port (task 00008) converts to/from native wchar_t / UTF-8. */
typedef uint16_t nsxml_char_t;
typedef uint16_t ns_char_t;
typedef uint16_t nsfilename_char_t;

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t Data4[8];
} GUID;
#endif
