/*

 macOS implementation of the nx string layer (see nxstring.h).

 The Win32 reference (win/nxstring.c) leans on the Windows API for everything:
 MultiByteToWideChar/WideCharToMultiByte for charset conversion, the process
 heap for allocation, PathCombineW, CompareString, _ui64tow, etc. None of that
 exists on macOS, and Win32 `wchar_t` is 16-bit while macOS `wchar_t` is 32-bit.

 This port therefore:
   - stores text internally as 16-bit UTF-16 code units (nx_utf16_t),
   - allocates with malloc/realloc/free (the heap handle is ignored),
   - carries its own small, self-contained UTF-8 <-> UTF-16 <-> UTF-32
     transcoders so it has no external dependency (no libiconv / CoreFoundation),
   - handles the ascii / latin1 / utf16le / utf16be charsets directly.

 arm64 (and all current Apple Silicon) is little-endian, so the internal UTF-16
 buffer is natively UTF-16LE: nx_charset_utf16le access is a direct pointer, as
 on Win32.

*/
#include "nxstring.h"
#include "foundation/error.h"
#include "foundation/atomics.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* UTF transcoders                                                    */
/* ------------------------------------------------------------------ */

#define NX_REPLACEMENT 0xFFFDu

/* Decode one UTF-8 sequence starting at s[0] (NUL-terminated input). Returns the
   code point and advances *s past the consumed bytes. Invalid bytes decode to
   U+FFFD and consume a single byte so the walk always makes progress. */
static uint32_t utf8_decode(const unsigned char **s)
{
	const unsigned char *p = *s;
	uint32_t c = p[0];

	if (c < 0x80) {
		*s = p + 1;
		return c;
	} else if ((c & 0xE0) == 0xC0) {
		if ((p[1] & 0xC0) == 0x80) {
			uint32_t cp = ((c & 0x1F) << 6) | (p[1] & 0x3F);
			*s = p + 2;
			return cp < 0x80 ? NX_REPLACEMENT : cp;
		}
	} else if ((c & 0xF0) == 0xE0) {
		if ((p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
			uint32_t cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
			*s = p + 3;
			return (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) ? NX_REPLACEMENT : cp;
		}
	} else if ((c & 0xF8) == 0xF0) {
		if ((p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
			uint32_t cp = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
			              ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
			*s = p + 4;
			return (cp < 0x10000 || cp > 0x10FFFF) ? NX_REPLACEMENT : cp;
		}
	}
	*s = p + 1;
	return NX_REPLACEMENT;
}

/* Encode a code point as UTF-8 into buf (must hold >= 4 bytes). Returns count. */
static size_t utf8_encode(uint32_t cp, char *buf)
{
	if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
		cp = NX_REPLACEMENT;
	if (cp < 0x80) {
		buf[0] = (char)cp;
		return 1;
	} else if (cp < 0x800) {
		buf[0] = (char)(0xC0 | (cp >> 6));
		buf[1] = (char)(0x80 | (cp & 0x3F));
		return 2;
	} else if (cp < 0x10000) {
		buf[0] = (char)(0xE0 | (cp >> 12));
		buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
		buf[2] = (char)(0x80 | (cp & 0x3F));
		return 3;
	}
	buf[0] = (char)(0xF0 | (cp >> 18));
	buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
	buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
	buf[3] = (char)(0x80 | (cp & 0x3F));
	return 4;
}

/* Append a code point to a UTF-16 buffer, writing a surrogate pair when needed.
   dst may be NULL for sizing-only; returns the number of code units required. */
static size_t utf16_put(uint32_t cp, nx_utf16_t *dst, size_t pos)
{
	if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
		cp = NX_REPLACEMENT;
	if (cp < 0x10000) {
		if (dst)
			dst[pos] = (nx_utf16_t)cp;
		return 1;
	}
	cp -= 0x10000;
	if (dst) {
		dst[pos] = (nx_utf16_t)(0xD800 + (cp >> 10));
		dst[pos + 1] = (nx_utf16_t)(0xDC00 + (cp & 0x3FF));
	}
	return 2;
}

/* Decode one UTF-16 code point from s (NUL-terminated), advancing *s. */
static uint32_t utf16_decode(const nx_utf16_t **s)
{
	const nx_utf16_t *p = *s;
	uint32_t c = p[0];
	if (c >= 0xD800 && c <= 0xDBFF && p[1] >= 0xDC00 && p[1] <= 0xDFFF) {
		uint32_t lo = p[1];
		*s = p + 2;
		return 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
	}
	*s = p + 1;
	return c;
}

/* number of UTF-16 code units needed to represent a UTF-8 string (no NUL) */
static size_t utf8_to_utf16_len(const char *str)
{
	const unsigned char *p = (const unsigned char *)str;
	size_t n = 0;
	while (*p) {
		uint32_t cp = utf8_decode(&p);
		n += utf16_put(cp, NULL, 0);
	}
	return n;
}

/* writes UTF-16 (no NUL); returns code units written */
static size_t utf8_to_utf16(const char *str, nx_utf16_t *dst)
{
	const unsigned char *p = (const unsigned char *)str;
	size_t n = 0;
	while (*p) {
		uint32_t cp = utf8_decode(&p);
		n += utf16_put(cp, dst, n);
	}
	return n;
}

/* native wchar_t (UTF-32 on macOS) -> internal UTF-16 length (no NUL) */
static size_t wchar_to_utf16_len(const wchar_t *str)
{
	size_t n = 0;
	for (const wchar_t *p = str; *p; ++p)
		n += utf16_put((uint32_t)*p, NULL, 0);
	return n;
}

static size_t wchar_to_utf16(const wchar_t *str, nx_utf16_t *dst)
{
	size_t n = 0;
	for (const wchar_t *p = str; *p; ++p)
		n += utf16_put((uint32_t)*p, dst, n);
	return n;
}

/* UTF-16 (len units) -> UTF-8 byte count (no NUL) */
static size_t utf16_to_utf8_size(const nx_utf16_t *src, size_t len)
{
	const nx_utf16_t *p = src;
	const nx_utf16_t *end = src + len;
	char tmp[4];
	size_t bytes = 0;
	while (p < end) {
		uint32_t cp = utf16_decode(&p);
		bytes += utf8_encode(cp, tmp);
	}
	return bytes;
}

/* UTF-16 (len units) -> UTF-8 into dst (capacity bytes, no NUL written). Returns
   bytes written; stops if it would overflow the capacity. */
static size_t utf16_to_utf8(const nx_utf16_t *src, size_t len, char *dst, size_t cap)
{
	const nx_utf16_t *p = src;
	const nx_utf16_t *end = src + len;
	char tmp[4];
	size_t bytes = 0;
	while (p < end) {
		uint32_t cp = utf16_decode(&p);
		size_t n = utf8_encode(cp, tmp);
		if (bytes + n > cap)
			break;
		memcpy(dst + bytes, tmp, n);
		bytes += n;
	}
	return bytes;
}

/* ------------------------------------------------------------------ */
/* Allocation                                                         */
/* ------------------------------------------------------------------ */

/* The heap handle is accepted for source compatibility with the Win32 API but
   is ignored: macOS has no per-process heap handle. */
int NXStringSetHeap(void *string_heap)
{
	(void)string_heap;
	return NErr_Success;
}

/* size in bytes for a string holding `characters` code units (+1 for the NUL),
   including the struct header. */
static size_t NXStringMallocSize(size_t characters)
{
	const nx_string_t dummy = NULL;
	size_t header = (size_t)&dummy->string[0] - (size_t)dummy;
	return header + (characters + 1) * sizeof(nx_utf16_t);
}

nx_string_t NXStringMallocWithHeap(void *heap, size_t characters)
{
	(void)heap;
	nx_string_t str = (nx_string_t)malloc(NXStringMallocSize(characters));
	if (str) {
		str->ref_count = 1;
		str->len = characters;
	}
	return str;
}

nx_string_t NXStringMalloc(size_t characters)
{
	return NXStringMallocWithHeap(NULL, characters);
}

nx_string_t NXStringRealloc(nx_string_t str, size_t characters)
{
	nx_string_t new_str = (nx_string_t)realloc(str, NXStringMallocSize(characters));
	/* on failure, kick back the original block (matches the Win32 port) */
	if (!new_str)
		return str;
	return new_str;
}

static int NXStringFree(void *heap, nx_string_t str)
{
	(void)heap;
	free(str);
	return NErr_Success;
}

/* ------------------------------------------------------------------ */
/* Construction                                                       */
/* ------------------------------------------------------------------ */

nx_string_t NXStringCreate(const wchar_t *str)
{
	if (!str || (size_t)str <= 65536)
		return 0;

	size_t size = wchar_to_utf16_len(str);
	nx_string_t nxstr = NXStringMalloc(size);
	if (nxstr) {
		wchar_to_utf16(str, nxstr->string);
		nxstr->string[size] = 0;
	}
	return nxstr;
}

nx_string_t NXStringCreateWithHeap(void *heap, const wchar_t *str)
{
	size_t size = wchar_to_utf16_len(str);
	nx_string_t nxstr = NXStringMallocWithHeap(heap, size);
	if (nxstr) {
		wchar_to_utf16(str, nxstr->string);
		nxstr->string[size] = 0;
	}
	return nxstr;
}

int NXStringCreateEmpty(nx_string_t *new_string)
{
	nx_string_t nxstr = NXStringMalloc(0);
	if (nxstr) {
		nxstr->string[0] = 0;
		*new_string = nxstr;
		return NErr_Success;
	}
	return NErr_OutOfMemory;
}

nx_string_t NXStringCreateFromUTF8(const char *str)
{
	if (!str)
		return 0;
	size_t size = utf8_to_utf16_len(str);
	nx_string_t nxstr = NXStringMalloc(size);
	if (nxstr) {
		utf8_to_utf16(str, nxstr->string);
		nxstr->string[size] = 0;
	}
	return nxstr;
}

int NXStringCreateWithUTF8(nx_string_t *new_value, const char *str)
{
	if (!str)
		return NErr_Empty;
	size_t size = utf8_to_utf16_len(str);
	nx_string_t nxstr = NXStringMalloc(size);
	if (!nxstr)
		return NErr_OutOfMemory;
	utf8_to_utf16(str, nxstr->string);
	nxstr->string[size] = 0;
	*new_value = nxstr;
	return NErr_Success;
}

/* On macOS the argument is a native (UTF-32) wide string; it is transcoded to
   the internal UTF-16 representation. (On Win32 wchar_t already *is* UTF-16.) */
int NXStringCreateWithUTF16(nx_string_t *new_value, const wchar_t *str)
{
	if (!str)
		return NErr_Empty;
	size_t size = wchar_to_utf16_len(str);
	nx_string_t nxstr = NXStringMalloc(size);
	if (!nxstr)
		return NErr_OutOfMemory;
	wchar_to_utf16(str, nxstr->string);
	nxstr->string[size] = 0;
	*new_value = nxstr;
	return NErr_Success;
}

int NXStringCreateWithCString(nx_string_t *new_value, const char *str, nx_charset_t charset)
{
	return NXStringCreateWithBytes(new_value, str, str ? strlen(str) : 0, charset);
}

nx_string_t NXStringRetain(nx_string_t string)
{
	if (!string)
		return 0;
	nx_atomic_inc(&string->ref_count);
	return string;
}

void NXStringRelease(nx_string_t string)
{
	if (string) {
		if (nx_atomic_dec(&string->ref_count) == 0)
			NXStringFree(NULL, string);
	}
}

nx_string_t NXStringCreateFromPath(const wchar_t *folder, const wchar_t *filename)
{
	size_t flen = wchar_to_utf16_len(folder);
	size_t nlen = wchar_to_utf16_len(filename);
	/* +1 for a possible separator */
	nx_string_t pathstr = NXStringMalloc(flen + nlen + 1);
	if (pathstr) {
		size_t pos = wchar_to_utf16(folder, pathstr->string);
		if (pos && pathstr->string[pos - 1] != '/' && pathstr->string[pos - 1] != '\\')
			pathstr->string[pos++] = '/';
		pos += wchar_to_utf16(filename, pathstr->string + pos);
		pathstr->string[pos] = 0;
		pathstr->len = pos;
	}
	return pathstr;
}

/* small ASCII-decimal renderer shared by the integer constructors */
static nx_string_t NXStringFromCDecimal(const char *cstr)
{
	size_t size = strlen(cstr);
	nx_string_t intstr = NXStringMalloc(size);
	if (intstr) {
		for (size_t i = 0; i < size; ++i)
			intstr->string[i] = (nx_utf16_t)(unsigned char)cstr[i];
		intstr->string[size] = 0;
		intstr->len = size;
	}
	return intstr;
}

nx_string_t NXStringCreateFromUInt64(uint64_t value)
{
	char buf[21];
	snprintf(buf, sizeof(buf), "%llu", (unsigned long long)value);
	return NXStringFromCDecimal(buf);
}

int NXStringCreateWithUInt64(nx_string_t *new_value, uint64_t value)
{
	nx_string_t s = NXStringCreateFromUInt64(value);
	if (!s)
		return NErr_OutOfMemory;
	*new_value = s;
	return NErr_Success;
}

int NXStringCreateWithInt64(nx_string_t *new_value, int64_t value)
{
	char buf[21];
	snprintf(buf, sizeof(buf), "%lld", (long long)value);
	nx_string_t s = NXStringFromCDecimal(buf);
	if (!s)
		return NErr_OutOfMemory;
	*new_value = s;
	return NErr_Success;
}

int NXStringCreateWithBytes(nx_string_t *new_string, const void *data, size_t len, nx_charset_t charset)
{
	nx_string_t nxstr;

	if (!len)
		return NXStringCreateEmpty(new_string);

	if (charset == nx_charset_utf16le || charset == nx_charset_utf16be) {
		size_t units = len / 2;
		nxstr = NXStringMalloc(units);
		if (!nxstr)
			return NErr_OutOfMemory;
		const unsigned char *b = (const unsigned char *)data;
		for (size_t i = 0; i < units; ++i) {
			if (charset == nx_charset_utf16le)
				nxstr->string[i] = (nx_utf16_t)(b[2 * i] | (b[2 * i + 1] << 8));
			else
				nxstr->string[i] = (nx_utf16_t)(b[2 * i + 1] | (b[2 * i] << 8));
		}
		nxstr->string[units] = 0;
		nxstr->len = units;
		*new_string = nxstr;
		return NErr_Success;
	} else if (charset == nx_charset_ascii || charset == nx_charset_latin1) {
		const unsigned char *b = (const unsigned char *)data;
		nxstr = NXStringMalloc(len);
		if (!nxstr)
			return NErr_OutOfMemory;
		for (size_t i = 0; i < len; ++i)
			nxstr->string[i] = (nx_utf16_t)b[i];
		nxstr->string[len] = 0;
		nxstr->len = len;
		*new_string = nxstr;
		return NErr_Success;
	} else {
		/* utf8 / system: data is not necessarily NUL-terminated, so copy it */
		char *tmp = (char *)malloc(len + 1);
		if (!tmp)
			return NErr_OutOfMemory;
		memcpy(tmp, data, len);
		tmp[len] = 0;
		size_t size = utf8_to_utf16_len(tmp);
		nxstr = NXStringMalloc(size);
		if (!nxstr) {
			free(tmp);
			return NErr_OutOfMemory;
		}
		utf8_to_utf16(tmp, nxstr->string);
		nxstr->string[size] = 0;
		nxstr->len = size;
		free(tmp);
		*new_string = nxstr;
		return NErr_Success;
	}
}

size_t NXStringGetLength(nx_string_t string)
{
	return (string ? string->len : 0);
}

/* ------------------------------------------------------------------ */
/* Keyword (ASCII) comparison                                         */
/* ------------------------------------------------------------------ */

int NXStringKeywordCompareWithCString(nx_string_t string, const char *compare_to)
{
	const nx_utf16_t *src = string->string;
	const char *dst = compare_to;
	int ret = 0;

	while (!(ret = (int)((*src & ~0x20) - (*dst & ~0x20))) && *dst) {
		++src, ++dst;
	}
	if (ret < 0)
		ret = -1;
	else if (ret > 0)
		ret = 1;
	return ret;
}

int NXStringKeywordCompare(nx_string_t string, nx_string_t compare_to)
{
	const nx_utf16_t *src = string->string;
	const nx_utf16_t *dst = compare_to->string;
	int ret = 0;

	while (!(ret = (int)((*src & ~0x20) - (*dst & ~0x20))) && *dst) {
		++src, ++dst;
	}
	if (ret < 0)
		ret = -1;
	else if (ret > 0)
		ret = 1;
	return ret;
}

int NXStringKeywordCaseCompare(nx_string_t string, nx_string_t compare_to)
{
	const nx_utf16_t *src = string->string;
	const nx_utf16_t *dst = compare_to->string;
	int ret = 0;

	while (!(ret = (int)(*src - *dst)) && *dst) {
		++src, ++dst;
	}
	if (ret < 0)
		ret = -1;
	else if (ret > 0)
		ret = 1;
	return ret;
}

int NXStringCreateBasePathFromFilename(nx_string_t filename, nx_string_t *basepath)
{
	size_t len = filename->len;
	while (len && filename->string[len - 1] != '\\' && filename->string[len - 1] != '/')
		len--;

	if (!len)
		return NErr_Empty;

	nx_string_t nxstr = NXStringMalloc(len);
	if (!nxstr)
		return NErr_OutOfMemory;

	memcpy(nxstr->string, filename->string, sizeof(nx_utf16_t) * len);
	nxstr->string[len] = 0;
	nxstr->len = len;
	*basepath = nxstr;
	return NErr_Success;
}

/* ------------------------------------------------------------------ */
/* Extraction                                                         */
/* ------------------------------------------------------------------ */

int NXStringGetCString(nx_string_t string, char *user_buffer, size_t user_buffer_length, const char **out_cstring, size_t *out_cstring_length)
{
	if (!string)
		return NErr_NullPointer;
	if (user_buffer_length == 0)
		return NErr_Insufficient;

	size_t size = utf16_to_utf8(string->string, string->len, user_buffer, user_buffer_length - 1);
	user_buffer[size] = 0;
	*out_cstring = user_buffer;
	*out_cstring_length = size;
	return NErr_Success;
}

/* Copy the (ASCII) UTF-16 string into a small char buffer for the numeric
   parsers. Non-ASCII code units are clamped to a byte. */
static void u16_to_ascii(const nx_utf16_t *src, char *dst, size_t cap)
{
	size_t i = 0;
	for (; src[i] && i + 1 < cap; ++i)
		dst[i] = (src[i] < 128) ? (char)src[i] : '?';
	dst[i] = 0;
}

int NXStringGetDoubleValue(nx_string_t string, double *value)
{
	if (!string)
		return NErr_NullPointer;
	char buf[64];
	u16_to_ascii(string->string, buf, sizeof(buf));
	*value = strtod(buf, 0);
	return NErr_Success;
}

int NXStringGetIntegerValue(nx_string_t string, int *value)
{
	if (!string)
		return NErr_NullPointer;
	char buf[32];
	u16_to_ascii(string->string, buf, sizeof(buf));
	*value = (int)strtol(buf, 0, 10);
	return NErr_Success;
}

int NXStringGetGUIDValue(nx_string_t string, GUID *out_guid)
{
	GUID guid;
	memset(&guid, 0, sizeof(guid));
	unsigned int Data1, Data2, Data3;
	unsigned int Data4[8] = {0};
	char buf[64];

	if (!string)
		return NErr_NullPointer;

	u16_to_ascii(string->string, buf, sizeof(buf));

	/* skip leading '{' / spaces */
	const char *p = buf;
	while (*p == '{' || *p == ' ')
		++p;

	if (sscanf(p, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
	           &Data1, &Data2, &Data3, &Data4[0], &Data4[1], &Data4[2], &Data4[3],
	           &Data4[4], &Data4[5], &Data4[6], &Data4[7]) != 11)
		return NErr_Malformed;

	guid.Data1 = Data1;
	guid.Data2 = (uint16_t)Data2;
	guid.Data3 = (uint16_t)Data3;
	for (int i = 0; i < 8; ++i)
		guid.Data4[i] = (uint8_t)Data4[i];

	*out_guid = guid;
	return NErr_Success;
}

int NXStringGetBytesSize(size_t *byte_count, nx_string_t string, nx_charset_t charset, int flags)
{
	if (charset == nx_charset_utf16le) {
		if (flags & nx_string_get_bytes_size_null_terminate)
			*byte_count = (string->len + 1) * sizeof(nx_utf16_t);
		else
			*byte_count = string->len * sizeof(nx_utf16_t);
		return NErr_DirectPointer;
	} else if (charset == nx_charset_utf16be) {
		*byte_count = (string->len + ((flags & nx_string_get_bytes_size_null_terminate) ? 1 : 0)) * sizeof(nx_utf16_t);
		return NErr_Success;
	} else if (charset == nx_charset_ascii || charset == nx_charset_latin1) {
		*byte_count = string->len + ((flags & nx_string_get_bytes_size_null_terminate) ? 1 : 0);
		return NErr_Success;
	} else {
		size_t size = utf16_to_utf8_size(string->string, string->len);
		if (flags & nx_string_get_bytes_size_null_terminate)
			size += 1;
		*byte_count = size;
		return NErr_Success;
	}
}

int NXStringGetBytesDirect(const void **bytes, size_t *length, nx_string_t string, nx_charset_t charset, int flags)
{
	if (charset == nx_charset_utf16le) {
		*bytes = string->string;
		if (length) {
			if (flags & nx_string_get_bytes_size_null_terminate)
				*length = (string->len + 1) * sizeof(nx_utf16_t);
			else
				*length = string->len * sizeof(nx_utf16_t);
		}
		return NErr_Success;
	}
	return NErr_Error;
}

int NXStringGetBytes(size_t *bytes_copied, nx_string_t string, void *bytes, size_t length, nx_charset_t charset, int flags)
{
	int null_terminate = (flags & nx_string_get_bytes_size_null_terminate) != 0;

	if (charset == nx_charset_utf16le || charset == nx_charset_utf16be) {
		size_t units = length / sizeof(nx_utf16_t);
		if (null_terminate) {
			if (units == 0)
				return NErr_Insufficient;
			units--;
		}
		if (units > string->len)
			units = string->len;

		unsigned char *out = (unsigned char *)bytes;
		for (size_t i = 0; i < units; ++i) {
			nx_utf16_t u = string->string[i];
			if (charset == nx_charset_utf16le) {
				out[2 * i] = (unsigned char)(u & 0xFF);
				out[2 * i + 1] = (unsigned char)(u >> 8);
			} else {
				out[2 * i] = (unsigned char)(u >> 8);
				out[2 * i + 1] = (unsigned char)(u & 0xFF);
			}
		}
		if (null_terminate) {
			out[2 * units] = 0;
			out[2 * units + 1] = 0;
			units++;
		}
		if (bytes_copied)
			*bytes_copied = units * sizeof(nx_utf16_t);
		return NErr_Success;
	} else if (charset == nx_charset_ascii || charset == nx_charset_latin1) {
		size_t cap = length;
		if (null_terminate) {
			if (cap == 0)
				return NErr_Insufficient;
			cap--;
		}
		size_t n = (cap > string->len) ? string->len : cap;
		unsigned char *out = (unsigned char *)bytes;
		for (size_t i = 0; i < n; ++i)
			out[i] = (string->string[i] < 256) ? (unsigned char)string->string[i] : '?';
		if (null_terminate)
			out[n] = 0;
		if (bytes_copied)
			*bytes_copied = n + (null_terminate ? 1 : 0);
		return NErr_Success;
	} else {
		size_t cap = length;
		if (null_terminate) {
			if (cap == 0)
				return NErr_Insufficient;
			cap--;
		}
		size_t size = utf16_to_utf8(string->string, string->len, (char *)bytes, cap);
		if (null_terminate)
			((char *)bytes)[size] = 0;
		if (bytes_copied)
			*bytes_copied = size + (null_terminate ? 1 : 0);
		return NErr_Success;
	}
}

/* Simplified locale-independent comparison. The Win32 port defers to
   CompareString with the user locale; here we do an ordinal comparison on the
   UTF-16 code units, optionally folding ASCII case. This matches Win32 for
   ASCII and gives a stable ordering elsewhere. */
nx_compare_result NXStringCompare(nx_string_t string1, nx_string_t string2, nx_compare_options options)
{
	int fold = (0 != (nx_compare_case_insensitive & options));
	const nx_utf16_t *a = string1->string;
	const nx_utf16_t *b = string2->string;

	for (;;) {
		nx_utf16_t ca = *a, cb = *b;
		if (fold) {
			if (ca >= 'A' && ca <= 'Z')
				ca += 32;
			if (cb >= 'A' && cb <= 'Z')
				cb += 32;
		}
		if (ca != cb)
			return (ca < cb) ? nx_compare_less_than : nx_compare_greater_than;
		if (ca == 0)
			return nx_compare_equal_to;
		++a, ++b;
	}
}

int NXStringCreateWithFormatting(nx_string_t *new_string, const char *format, ...)
{
	va_list v;
	va_start(v, format);
	int cch = vsnprintf(NULL, 0, format, v);
	va_end(v);
	if (cch < 0)
		return NErr_Error;

	char *temp = (char *)malloc((size_t)cch + 1);
	if (!temp)
		return NErr_OutOfMemory;

	va_start(v, format);
	vsnprintf(temp, (size_t)cch + 1, format, v);
	va_end(v);

	int ret = NXStringCreateWithUTF8(new_string, temp);
	free(temp);
	return ret;
}
