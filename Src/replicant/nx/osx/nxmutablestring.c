/*

 macOS implementation of the nx mutable-string helper (see nxmutablestring.h).

 Counterpart to win/nxmutablestring.c. Closely coupled to the nx_string_t
 implementation (it grows the underlying buffer in place via NXStringRealloc).
 The Win32 version allocated the wrapper out of the process heap; here it uses
 malloc/free. XML characters are 16-bit UTF-16 code units (nsxml_char_t), the
 same width as the internal nx_string_t storage, so they copy directly.

*/
#include "nxmutablestring.h"
#include "foundation/error.h"
#include <stdlib.h>
#include <string.h>

void NXMutableStringDestroy(nx_mutable_string_t mutable_string)
{
	if (mutable_string) {
		if (mutable_string->nx_string_data)
			NXStringRelease(mutable_string->nx_string_data);
		free(mutable_string);
	}
}

nx_mutable_string_t NXMutableStringCreateFromXML(const nsxml_char_t *characters, size_t num_characters)
{
	nx_mutable_string_t mutable_string = (nx_mutable_string_t)malloc(sizeof(nx_mutable_string_struct_t));
	if (!mutable_string)
		return 0;
	if (NXStringCreateWithBytes(&mutable_string->nx_string_data, characters, num_characters * 2, nx_charset_utf16le) != NErr_Success) {
		free(mutable_string);
		return 0;
	}
	mutable_string->allocation_length = num_characters;
	return mutable_string;
}

int NXMutableStringGrowFromXML(nx_mutable_string_t mutable_string, const nsxml_char_t *characters, size_t num_characters)
{
	if (mutable_string->nx_string_data->len + num_characters + 1 > mutable_string->allocation_length) {
		nx_string_t new_string = NXStringRealloc(mutable_string->nx_string_data, mutable_string->nx_string_data->len + num_characters + 1);
		if (!new_string)
			return NErr_OutOfMemory;
		mutable_string->nx_string_data = new_string;
		mutable_string->allocation_length = mutable_string->nx_string_data->len + num_characters + 1;
	}
	memcpy(mutable_string->nx_string_data->string + mutable_string->nx_string_data->len, characters, num_characters * sizeof(nsxml_char_t));
	mutable_string->nx_string_data->len += num_characters;
	mutable_string->nx_string_data->string[mutable_string->nx_string_data->len] = 0; /* null terminate */

	return NErr_Success;
}
