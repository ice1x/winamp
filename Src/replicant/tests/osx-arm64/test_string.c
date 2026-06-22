/*
 * test_string.c - osx-arm64 port of nx/nxstring.h
 *
 * The Win32 nx_string_t stores text as 16-bit wchar_t (UTF-16) and relies on
 * the Windows API (MultiByteToWideChar/WideCharToMultiByte/CompareString/...).
 * macOS wchar_t is 32-bit, so the port stores explicit 16-bit UTF-16 code
 * units and carries its own UTF-8 <-> UTF-16 <-> UTF-32 transcoders. These
 * tests exercise that round-tripping (including a 2-byte BMP char and a 4-byte
 * astral char that needs a UTF-16 surrogate pair), ref-counting, the keyword/
 * ordinal comparisons, the integer/GUID/numeric helpers, and the charset byte
 * conversions. Compiled with -arch arm64 so the __APPLE__ dispatchers select
 * the osx sources.
 */
#include "nx/nxstring.h"
#include "foundation/error.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL: %s (line %d)\n", #cond, __LINE__);                       \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

/* "héllo 😀" : h, U+00E9 (2-byte UTF-8), l, l, o, space, U+1F600 (4-byte UTF-8,
   surrogate pair in UTF-16). 11 UTF-8 bytes, 8 UTF-16 code units. */
static const char *kMixed = "h\xC3\xA9llo \xF0\x9F\x98\x80";

int main(void) {
  printf("test_string (osx-arm64)\n");

  /* ---- UTF-8 round trip, including BMP + astral characters ---- */
  nx_string_t s = NXStringCreateFromUTF8(kMixed);
  CHECK(s != NULL);
  CHECK(NXStringGetLength(s) == 8); /* surrogate pair counts as two units */

  size_t need = 0;
  CHECK(NXStringGetBytesSize(&need, s, nx_charset_utf8,
                             nx_string_get_bytes_size_null_terminate) ==
        NErr_Success);
  CHECK(need == strlen(kMixed) + 1);

  char buf[64];
  size_t copied = 0;
  CHECK(NXStringGetBytes(&copied, s, buf, sizeof(buf), nx_charset_utf8,
                         nx_string_get_bytes_size_null_terminate) ==
        NErr_Success);
  CHECK(strcmp(buf, kMixed) == 0);
  CHECK(copied == strlen(kMixed) + 1);

  /* GetCString gives the same UTF-8 bytes on macOS (system locale is UTF-8) */
  const char *cstr = NULL;
  size_t clen = 0;
  char cbuf[64];
  CHECK(NXStringGetCString(s, cbuf, sizeof(cbuf), &cstr, &clen) == NErr_Success);
  CHECK(strcmp(cstr, kMixed) == 0);

  /* ---- native wchar_t (UTF-32) constructor must match the UTF-8 one ---- */
  nx_string_t w = NXStringCreate(L"héllo \U0001F600");
  CHECK(w != NULL);
  CHECK(NXStringGetLength(w) == 8);
  CHECK(NXStringCompare(s, w, nx_compare_default) == nx_compare_equal_to);
  nx_string_t w2 = NULL;
  CHECK(NXStringCreateWithUTF16(&w2, L"héllo \U0001F600") == NErr_Success);
  CHECK(NXStringCompare(s, w2, nx_compare_default) == nx_compare_equal_to);

  /* ---- utf16le direct pointer; surrogate pair must be present ---- */
  const void *direct = NULL;
  size_t dlen = 0;
  CHECK(NXStringGetBytesDirect(&direct, &dlen, s, nx_charset_utf16le, 0) ==
        NErr_Success);
  CHECK(dlen == 8 * sizeof(uint16_t));
  const uint16_t *u16 = (const uint16_t *)direct;
  CHECK(u16[0] == 'h');
  CHECK(u16[1] == 0x00E9);
  CHECK(u16[6] >= 0xD800 && u16[6] <= 0xDBFF); /* high surrogate */
  CHECK(u16[7] >= 0xDC00 && u16[7] <= 0xDFFF); /* low surrogate */

  /* CreateWithBytes(utf16le) should rebuild an equal string */
  nx_string_t rebuilt = NULL;
  CHECK(NXStringCreateWithBytes(&rebuilt, direct, dlen, nx_charset_utf16le) ==
        NErr_Success);
  CHECK(NXStringCompare(s, rebuilt, nx_compare_default) == nx_compare_equal_to);

  /* ---- utf16be byte order is the reverse of utf16le ---- */
  unsigned char be[16];
  size_t becopied = 0;
  CHECK(NXStringGetBytes(&becopied, s, be, sizeof(be), nx_charset_utf16be, 0) ==
        NErr_Success);
  CHECK(becopied == 8 * sizeof(uint16_t));
  CHECK(be[0] == 0x00 && be[1] == 'h'); /* big-endian 'h' */

  /* ---- ref counting: retain then release twice keeps it alive once ---- */
  nx_string_t empty = NULL;
  CHECK(NXStringCreateEmpty(&empty) == NErr_Success);
  CHECK(NXStringGetLength(empty) == 0);
  CHECK(NXStringRetain(empty) == empty);
  NXStringRelease(empty); /* drop the retain */
  NXStringRelease(empty); /* drop the create */

  /* ---- keyword (ASCII, case-insensitive) comparisons ---- */
  nx_string_t kw = NXStringCreateFromUTF8("Hello");
  CHECK(NXStringKeywordCompareWithCString(kw, "hello") == 0);
  CHECK(NXStringKeywordCompareWithCString(kw, "world") != 0);
  nx_string_t kw2 = NXStringCreateFromUTF8("HELLO");
  CHECK(NXStringKeywordCompare(kw, kw2) == 0);
  CHECK(NXStringKeywordCaseCompare(kw, kw2) != 0); /* case-sensitive differs */

  /* ---- ordinal compare, case folding ---- */
  nx_string_t lower = NXStringCreateFromUTF8("apple");
  nx_string_t upper = NXStringCreateFromUTF8("APPLE");
  CHECK(NXStringCompare(lower, upper, nx_compare_default) ==
        nx_compare_greater_than); /* 'a' > 'A' ordinally */
  CHECK(NXStringCompare(lower, upper, nx_compare_case_insensitive) ==
        nx_compare_equal_to);

  /* ---- integer / numeric helpers ---- */
  nx_string_t num = NULL;
  CHECK(NXStringCreateWithUInt64(&num, 18446744073709551615ULL) == NErr_Success);
  char nbuf[32];
  size_t nlen = 0;
  const char *ncstr = NULL;
  NXStringGetCString(num, nbuf, sizeof(nbuf), &ncstr, &nlen);
  CHECK(strcmp(ncstr, "18446744073709551615") == 0);

  nx_string_t neg = NULL;
  CHECK(NXStringCreateWithInt64(&neg, -1234) == NErr_Success);
  int iv = 0;
  CHECK(NXStringGetIntegerValue(neg, &iv) == NErr_Success);
  CHECK(iv == -1234);

  nx_string_t dbl = NXStringCreateFromUTF8("3.14159");
  double dv = 0;
  CHECK(NXStringGetDoubleValue(dbl, &dv) == NErr_Success);
  CHECK(dv > 3.1415 && dv < 3.1416);

  /* ---- GUID parsing ---- */
  nx_string_t guidstr =
      NXStringCreateFromUTF8("1B3CA60C-DA98-4826-B4A9-D79748A5FD73");
  GUID g;
  CHECK(NXStringGetGUIDValue(guidstr, &g) == NErr_Success);
  CHECK(g.Data1 == 0x1B3CA60Cu);
  CHECK(g.Data2 == 0xDA98);
  CHECK(g.Data3 == 0x4826);
  CHECK(g.Data4[0] == 0xB4 && g.Data4[7] == 0x73);

  /* ---- path construction and base-path extraction ---- */
  nx_string_t path = NXStringCreateFromPath(L"/Users/me", L"track.mp3");
  char pbuf[64];
  const char *pcstr = NULL;
  size_t plen = 0;
  NXStringGetCString(path, pbuf, sizeof(pbuf), &pcstr, &plen);
  CHECK(strcmp(pcstr, "/Users/me/track.mp3") == 0);

  nx_string_t base = NULL;
  CHECK(NXStringCreateBasePathFromFilename(path, &base) == NErr_Success);
  char bbuf[64];
  const char *bcstr = NULL;
  size_t blen = 0;
  NXStringGetCString(base, bbuf, sizeof(bbuf), &bcstr, &blen);
  CHECK(strcmp(bcstr, "/Users/me/") == 0);

  /* ---- formatting ---- */
  nx_string_t fmt = NULL;
  CHECK(NXStringCreateWithFormatting(&fmt, "%s=%d", "vol", 42) == NErr_Success);
  char fbuf[32];
  const char *fcstr = NULL;
  size_t flen = 0;
  NXStringGetCString(fmt, fbuf, sizeof(fbuf), &fcstr, &flen);
  CHECK(strcmp(fcstr, "vol=42") == 0);

  /* ---- ascii / latin1 byte extraction ---- */
  nx_string_t lat = NXStringCreateFromUTF8("caf\xC3\xA9"); /* "café" */
  unsigned char l1[8];
  size_t l1copied = 0;
  CHECK(NXStringGetBytes(&l1copied, lat, l1, sizeof(l1), nx_charset_latin1,
                         nx_string_get_bytes_size_null_terminate) ==
        NErr_Success);
  CHECK(l1[3] == 0xE9); /* é is 0xE9 in latin-1 */
  CHECK(l1[4] == 0x00); /* null terminated */

  NXStringRelease(s);
  NXStringRelease(w);
  NXStringRelease(w2);
  NXStringRelease(rebuilt);
  NXStringRelease(kw);
  NXStringRelease(kw2);
  NXStringRelease(lower);
  NXStringRelease(upper);
  NXStringRelease(num);
  NXStringRelease(neg);
  NXStringRelease(dbl);
  NXStringRelease(guidstr);
  NXStringRelease(path);
  NXStringRelease(base);
  NXStringRelease(fmt);
  NXStringRelease(lat);

  if (failures == 0) {
    printf("  OK\n");
    return 0;
  }
  printf("  %d failure(s)\n", failures);
  return 1;
}
