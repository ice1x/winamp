/*
 * test_types.c - osx-arm64 port of replicant foundation/types.h
 *
 * Verifies the fixed-width integer / colour / GUID typedefs resolve to the
 * sizes the rest of the replicant core assumes. Compiled with `-arch arm64`.
 */
#include "foundation/types.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL: %s (line %d)\n", #cond, __LINE__);                       \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

int main(void) {
  printf("test_types (osx-arm64)\n");

  /* fixed-width stdint types */
  CHECK(sizeof(uint8_t) == 1);
  CHECK(sizeof(uint16_t) == 2);
  CHECK(sizeof(uint32_t) == 4);
  CHECK(sizeof(uint64_t) == 8);
  CHECK(sizeof(int64_t) == 8);

  /* colour types must keep their packed pixel widths regardless of platform
     `long` width (on macOS `unsigned long` is 64-bit, so they must NOT be
     typedef'd to long the way the Win32 header does). */
  CHECK(sizeof(ARGB32) == 4);
  CHECK(sizeof(RGB32) == 4);
  CHECK(sizeof(ARGB16) == 2);
  CHECK(sizeof(RGB16) == 2);
  CHECK(sizeof(FOURCC) == 4);

  /* small helper integer aliases */
  CHECK(sizeof(UINT) == 4);
  CHECK(sizeof(SINT) == 4);
  CHECK(sizeof(UCHAR) == 1);
  CHECK(sizeof(SCHAR) == 1);

  /* string element width: replicant treats ns_char_t as a UTF-16 code unit */
  CHECK(sizeof(ns_char_t) == 2);

  /* GUID must be a 16-byte POD matching the Win32 layout */
  CHECK(sizeof(GUID) == 16);
  {
    GUID g = {0x11223344,
              0x5566,
              0x7788,
              {0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00}};
    CHECK(g.Data1 == 0x11223344u);
    CHECK(g.Data2 == 0x5566u);
    CHECK(g.Data3 == 0x7788u);
    CHECK(g.Data4[0] == 0x99);
    CHECK(g.Data4[7] == 0x00);
  }

  if (failures == 0) {
    printf("  OK\n");
    return 0;
  }
  printf("  %d failure(s)\n", failures);
  return 1;
}
