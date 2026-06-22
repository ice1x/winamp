/*
 * test_mutablestring.c - osx-arm64 port of nx/nxmutablestring.h
 *
 * NXMutableString grows an nx_string_t in place from successive runs of UTF-16
 * (nsxml_char_t) characters, as the XML parser feeds them. Verifies the initial
 * build, growth across the initial allocation boundary, and that the underlying
 * nx_string stays NUL-terminated with the right length. Compiled with
 * -arch arm64 so the __APPLE__ dispatchers select the osx sources.
 */
#include "nx/nxmutablestring.h"
#include "foundation/error.h"
#include <stdio.h>

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL: %s (line %d)\n", #cond, __LINE__);                       \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

int main(void) {
  printf("test_mutablestring (osx-arm64)\n");

  /* nsxml_char_t is a 16-bit UTF-16 code unit */
  const nsxml_char_t hello[] = {'h', 'e', 'l', 'l', 'o'};
  const nsxml_char_t world[] = {' ', 'w', 'o', 'r', 'l', 'd', '!'};

  nx_mutable_string_t m = NXMutableStringCreateFromXML(hello, 5);
  CHECK(m != NULL);
  CHECK(m->nx_string_data != NULL);
  CHECK(m->nx_string_data->len == 5);
  CHECK(m->nx_string_data->string[5] == 0);
  CHECK(m->nx_string_data->string[0] == 'h');

  /* growth forces a realloc past the initial allocation_length */
  CHECK(NXMutableStringGrowFromXML(m, world, 7) == NErr_Success);
  CHECK(m->nx_string_data->len == 12);
  CHECK(m->nx_string_data->string[12] == 0);
  CHECK(m->nx_string_data->string[5] == ' ');
  CHECK(m->nx_string_data->string[11] == '!');

  /* a second growth keeps accumulating */
  const nsxml_char_t more[] = {'?'};
  CHECK(NXMutableStringGrowFromXML(m, more, 1) == NErr_Success);
  CHECK(m->nx_string_data->len == 13);
  CHECK(m->nx_string_data->string[12] == '?');
  CHECK(m->nx_string_data->string[13] == 0);

  NXMutableStringDestroy(m);

  if (failures == 0) {
    printf("  OK\n");
    return 0;
  }
  printf("  %d failure(s)\n", failures);
  return 1;
}
