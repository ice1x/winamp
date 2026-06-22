/*

 macOS implementation of the nx mutable-string helper.

 Counterpart to win/nxmutablestring.h. Identical API; the implementation
 (nxmutablestring.c) is decoupled from the Win32 heap and built on top of the
 osx nx_string_t (UTF-16) primitives. The XML character type nsxml_char_t is a
 16-bit UTF-16 code unit (foundation/osx-arm64/types.h).

*/
#pragma once

#include "../../foundation/types.h"
#include "../../nx/nxstring.h"
#include "../../nx/nxapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nx_mutable_string_struct_t
{
	size_t allocation_length;
	nx_string_t nx_string_data;
} nx_mutable_string_struct_t, *nx_mutable_string_t;

NX_API nx_mutable_string_t NXMutableStringCreateFromXML(const nsxml_char_t *characters, size_t num_characters);
NX_API void NXMutableStringDestroy(nx_mutable_string_t mutable_string);
NX_API int NXMutableStringGrowFromXML(nx_mutable_string_t mutable_string, const nsxml_char_t *characters, size_t num_characters);

#ifdef __cplusplus
}
#endif
